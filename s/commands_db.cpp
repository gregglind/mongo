// commands_db.cpp

/**
*    Copyright (C) 2008 10gen Inc.
*
*    This program is free software: you can redistribute it and/or  modify
*    it under the terms of the GNU Affero General Public License, version 3,
*    as published by the Free Software Foundation.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU Affero General Public License for more details.
*
*    You should have received a copy of the GNU Affero General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


/**
   these are commands that live in mongod
   mostly around shard management and checking
 */

#include "stdafx.h"
#include <map>
#include <string>

#include "../db/commands.h"
#include "../db/jsobj.h"

using namespace std;

namespace mongo {
    
    typedef map<string,unsigned long long> NSVersions;
    
    NSVersions myVersions;
    boost::thread_specific_ptr<NSVersions> clientShardVersions;
    string shardConfigServer;


    class MongodShardCommand : public Command {
    public:
        MongodShardCommand( const char * n ) : Command( n ){
        }
        virtual bool slaveOk(){
            return false;
        }
        virtual bool adminOnly() {
            return true;
        }
    };

    // setShardVersion( ns )
    
    class SetShardVersion : public MongodShardCommand {
    public:
        SetShardVersion() : MongodShardCommand("setShardVersion"){}

        virtual void help( stringstream& help ) const {
            help << " example: { setShardVersion : 'alleyinsider.foo' , version : 1 , configdb : '' } ";
        }
        
        bool run(const char *cmdns, BSONObj& cmdObj, string& errmsg, BSONObjBuilder& result, bool){
            
            string configdb = cmdObj["configdb"].valuestrsafe();
            { // configdb checking
                if ( configdb.size() == 0 ){
                    errmsg = "no configdb";
                    return false;
                }
                
                if ( shardConfigServer.size() == 0 ){
                    shardConfigServer = configdb;
                }
                else if ( shardConfigServer != configdb ){
                    errmsg = "specified a different configdb!";
                    return false;
                }
            }
            
            unsigned long long version;
            {
                BSONElement e = cmdObj["version"];
                if ( e.eoo() ){
                    errmsg = "no version";
                    return false;
                }
                else if ( e.isNumber() )
                    version = (unsigned long long)e.number();
                else if ( e.type() == Date || e.type() == Timestamp )
                    version = e.date();
                else {
                    errmsg = "version is not a numberic type";
                    return false;
                }
                
            }

            NSVersions * versions = clientShardVersions.get();
            
            if ( ! versions ){
                log(1) << "entering shard mode for connection" << endl;
                versions = new NSVersions();
                clientShardVersions.reset( versions );
            }
            
            string ns = cmdObj["setShardVersion"].valuestrsafe();
            if ( ns.size() == 0 ){
                errmsg = "need to speciy fully namespace";
                return false;
            }

            unsigned long long& oldVersion = (*versions)[ns];
            if ( version < oldVersion ){
                errmsg = "going to older version for you!";
                return false;
            }

            unsigned long long& myVersion = myVersions[ns];
            if ( version < myVersion ){
                errmsg = "going to older version for global";
                return false;
            }
            
            result.appendTimestamp( "oldVersion" , oldVersion );
            oldVersion = version;
            myVersion = version;

            result.append( "ok" , 1 );
            return 1;
        }
        
    } setShardVersion;
    
    class GetShardVersion : public MongodShardCommand {
    public:
        GetShardVersion() : MongodShardCommand("getShardVersion"){}

        virtual void help( stringstream& help ) const {
            help << " example: { getShardVersion : 'alleyinsider.foo'  } ";
        }

        bool run(const char *cmdns, BSONObj& cmdObj, string& errmsg, BSONObjBuilder& result, bool){
            string ns = cmdObj["getShardVersion"].valuestrsafe();
            if ( ns.size() == 0 ){
                errmsg = "need to speciy fully namespace";
                return false;
            }
            
            result.appendTimestamp( "global" , myVersions[ns] );
            if ( clientShardVersions.get() )
                result.appendTimestamp( "mine" , (*clientShardVersions.get())[ns] );
            else 
                result.appendTimestamp( "mine" , 0 );
            
            result.append( "ok" , 1 );
            return true;
        }
        
    } getShardVersion;


}