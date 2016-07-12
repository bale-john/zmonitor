#include <fstream>
#include "ConstDef.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include "Config.h"
#include <unistd.h>
#include "Util.h"
#include "Log.h"
using namespace std;

Config* Config::_instance = NULL;

Config::Config(){
	resetConfig();
}

Config::~Config(){
	delete _instance;
}

Config* Config::getInstance(){
	if (_instance == NULL){
		_instance = new Config();
		_instance->load();
		//reload the config result in the change of loglevel in Log
		Log::init(_instance->getLogLevel());	
	}
	return _instance;
}

int Config::resetConfig(){
	_autoStart = 1;
    _daemonMode = 0;
    _logLevel = 2;
    _zkHost = "127.0.0.1:2181";
    _zkLogPath = ""; 
    _monitorHostname = ""; 
    _connRetryCount = 3;
    _scanInterval = 3;
    _serviceMap.clear();
    _zkRecvTimeout = 3000;
    serviceFatherStatus.clear();
	return 0;
}

int Config::setValueInt(const string& key, const string& value){
	int intValue = atoi(value.c_str());
	if (key == daemonMode) {
		if (intValue != 0) {
			_daemonMode = 1;
		}
	}
	else if (key == autoStart){
		if (intValue == 0) {
			_autoStart = 0;
		}
	}
	else if (key == logLevel){
		if (intValue >= minLogLevel && intValue <= maxLogLevel) {
			_logLevel = intValue;
		}
	}
	else if (key == connRetryCount){
		if (intValue > 0) {
			_connRetryCount = intValue;
		}
	}
	else if (key == scanInterval){
		if (intValue > 0) {
			_scanInterval = intValue;
		}
	}
	return 0;
}

int Config::setValueStr(const string& key, const string& value){
	if (key == instanceName){
		_instanceName = value;
	}
	else if (key == zkLogPath){
		_zkLogPath = value;
	}
	//find the zk host this monitor should focus on. Their idc should be the same
	else if (key.substr(0, zkHost.length()) == zkHost){
		char hostname[512] = {0};
		if (gethostname(hostname, sizeof(hostname)) != 0) {
			LOG(LOG_ERROR, "get host name failed");
			return -1;
		}
		string idc = key.substr(zkHost.length());
		vector<string> singleWord;
		singleWord = Util::split(string(hostname), '.');
		size_t i = 0;
		for (; i < singleWord.size(); ++i){
			if (singleWord[i] == idc){
				_zkHost = value;
				break;
			}
		}
        //no need to log this or there will be a lot error because config file has other idc
        /*
		if (i == singleWord.size()){
			LOG(LOG_ERROR, "idc not found");
			return -1;
		}
        */
	}
	return 0;
}

//这里面的serviceItem是什么用的
int Config::load(){
	ifstream file;
	file.open(confPath);

	if (file.good()){
		resetConfig();
		string line;
		while (getline(file, line)) {
			Util::trim(line);
			if (line.size() <= 0 || line[0] == '#') {
				continue;
			}
			size_t pos = line.find('=');
			if (pos == string::npos){
				continue;
			}
			//get the key
			string key = line.substr(0, pos);
			Util::trim(key);
			if (key.size() == 0 || key[0] == '#') {
				continue;
			}
			//get the value
			string value = line.substr(pos + 1);
			Util::trim(value);
			setValueInt(key, value);
			setValueStr(key, value);
		}
	} 
	else {
		LOG(LOG_ERROR, "config file open wrong");
	}
	file.close();
	return 0;
}

int Config::getLogLevel(){
	return _logLevel;
}

int Config::isDaemonMode(){
	return _daemonMode;
}

string Config::getMonitorHostname(){
	return _monitorHostname;
}

int Config::isAutoStart(){
	return _autoStart;
}

int Config::getConnRetryCount(){
	return _connRetryCount;
}

int Config::getScanInterval(){
	return _scanInterval;
}

string Config::getInstanceName(){
	return _instanceName;
}

string Config::getZkHost(){
	return _zkHost;
}

string Config::getZkLogPath(){
	return _zkLogPath;
}

void Config::clearServiceMap() {
	_serviceMap.clear();
}

int Config::getZkRecvTimeout() {
	return _zkRecvTimeout;
}

string Config::getNodeList() {
	//todo should do something to judge weather instanceName is null
	return LOCK_ROOT_DIR + SLASH + _instanceName + SLASH + NODE_LIST;
}

string Config::getMonitorList() {
	//todo
	return LOCK_ROOT_DIR + SLASH + _instanceName + SLASH + MONITOR_LIST;
}

int Config::printMap() {
	for (auto it = _serviceMap.begin(); it != _serviceMap.end(); ++it) {
		cout << it->first << endl;
		cout << "host: " << (it->second).getHost() << endl;
		cout << "port: " << (it->second).getPort() << endl;
		cout << "service father: " << (it->second).getServiceFather() << endl;
		cout << "status: " << (it->second).getStatus() << endl;
	}
    return 0;
}


int Config::addService(string ipPath, ServiceItem serviceItem) {
	//这里还需要加锁

    _serviceMap[ipPath] = serviceItem;
    return 0;
}

map<string, ServiceItem> Config::getServiceMap() {
	return _serviceMap;
}

int Config::setServiceFatherToIp(unordered_map<string, unordered_set<string>> sft) {
	serviceFatherToIp = sft;
    return 0;
}

unordered_map<string, unordered_set<string>>& Config::getServiceFatherToIp() {
	return serviceFatherToIp;
}

int Config::setServiceMap(string node, int val) {
	//todo 同样缺异常判断，比如找不到怎么办啊什么的，还有这里锁怎么加？
	ServiceItem item = _serviceMap[node];
	item.setStatus(val);
	return 0;
}

//对这种和较多类有关系的数据结构，一定要注意是否需要加锁
int Config::modifyServiceFatherStatus(const string& serviceFather, int status, int op) {
	serviceFatherStatus[serviceFather][status + 1] += op;
	return 0;
}

int Config::getServiceFatherStatus(const string& serviceFather, int status) {
	return serviceFatherStatus[serviceFather][status + 1];
}

int Config::modifyServiceFatherStatus(const string& serviceFather, vector<int>& statusv) {
	serviceFatherStatus[serviceFather] = statusv;
	return 0;
}