/*
  FSWebServer - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov. All rights reserved.
  This file is part of the ESP8266WebServer library for Arduino environment.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp8266fs.local/edit; done

  access the sample web page at http://esp8266fs.local
  edit the page by going to http://esp8266fs.local/edit
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include "FSbrowser.h"
#define DBG_OUTPUT_PORT //Serial
//holds the current upload
//File fsUploadFile;

void FSbrowser::fsset(ESP8266WebServer *server) {
  _server = server;
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename,ESP8266WebServer *_server){
  if(_server->hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool FSbrowser::handleFileRead(String path){
  //DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = _server->streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void FSbrowser::handleFileUpload(){
  if(_server->uri() != "/edit") return;
  HTTPUpload& upload = _server->upload();
  if(upload.status == UPLOAD_FILE_START){
    String filename = upload.filename;
    if(!filename.startsWith("/")) filename = "/"+filename;
    //DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if(upload.status == UPLOAD_FILE_WRITE){
    //DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if(fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if(upload.status == UPLOAD_FILE_END){
    if(fsUploadFile)
      fsUploadFile.close();
    //DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

void FSbrowser::handleFileDelete(){
  if(_server->args() == 0) return _server->send(500, "text/plain", "BAD ARGS");
  String path = _server->arg(0);
  //DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if(path == "/")
    return _server->send(500, "text/plain", "BAD PATH");
  if(!SPIFFS.exists(path))
    return _server->send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  _server->send(200, "text/plain", "");
  path = String();
}

void FSbrowser::handleFileCreate(){
  if(_server->args() == 0)
    return _server->send(500, "text/plain", "BAD ARGS");
  String path = _server->arg(0);
  //DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if(path == "/")
    return _server->send(500, "text/plain", "BAD PATH");
  if(SPIFFS.exists(path))
    return _server->send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if(file)
    file.close();
  else
    return _server->send(500, "text/plain", "CREATE FAILED");
  _server->send(200, "text/plain", "");
  path = String();
}

void FSbrowser::handleFileList() {
  if(!_server->hasArg("dir")) {_server->send(500, "text/plain", "BAD ARGS"); return;}

  String path = _server->arg("dir");
  //DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while(dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }

  output += "]";
  _server->send(200, "text/json", output);
}

void FSbrowser::fssetup(void){
  //DBG_OUTPUT_PORT.begin(115200);
  //DBG_OUTPUT_PORT.print("\n");
  //DBG_OUTPUT_PORT.setDebugOutput(true);
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      //DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
    }
    //DBG_OUTPUT_PORT.printf("\n");
  }

/*
  //WIFI INIT
  //DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //DBG_OUTPUT_PORT.print(".");
  }
  //DBG_OUTPUT_PORT.println("");
  //DBG_OUTPUT_PORT.print("Connected! IP address: ");
  //DBG_OUTPUT_PORT.println(WiFi.localIP());

  MDNS.begin(host);
  //DBG_OUTPUT_PORT.print("Open http://");
  //DBG_OUTPUT_PORT.print(host);
  //DBG_OUTPUT_PORT.println(".local/edit to see the file browser");
*/

  //SERVER INIT
  //list directory
  _server->on("/list", HTTP_GET, handleFileList);
  //load editor
  _server->on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) _server->send(404, "text/plain", "FileNotFound");
  });
  //create file
  _server->on("/edit", HTTP_PUT, handleFileCreate);
  //delete file
  _server->on("/edit", HTTP_DELETE, handleFileDelete);
  //first callback is called after the request has ended with all parsed arguments
  //second callback handles file uploads at that location
  _server->on("/edit", HTTP_POST, [](){ _server->send(200, "text/plain", ""); }, handleFileUpload);

  //called when the url is not defined here
  //use it to load content from SPIFFS
  _server->onNotFound([](){
    if(!handleFileRead(_server->uri()))
      _server->send(404, "text/plain", "FileNotFound");
  });

  //get heap status, analog input value and all GPIO statuses in one json call
  _server->on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    _server->send(200, "text/json", json);
    json = String();
  });
  _server->begin();
  //DBG_OUTPUT_PORT.println("HTTP server started");

}

void FSbrowser::fsloop(void){
  _server->handleClient();
}