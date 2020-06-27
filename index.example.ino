#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <FirebaseArduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

#define FIREBASE_HOST ""
#define FIREBASE_AUTH ""
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

String sensorName = "SENSOR_1";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void setup() {
  Serial.begin(9600);

  pinMode( D1, INPUT);

  // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("\nConectando\n");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println(".");
    delay(500);
  }
  Serial.println();
  Serial.print("Conectado! Endereço:  ");
  Serial.println(WiFi.localIP());

  timeClient.begin();

  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  
}

void loop() {
  byte val = digitalRead( D1);
  
  if( val == LOW) {
    Serial.println("Motion detected!");
    handleMotionDetected();
    delay(10000);
  }
}

void handleMotionDetected() {
    
  String alarmState = Firebase.getString("alarm_state/alarm_state");
  
  if(alarmState == "on") {
    while(!timeClient.update()) {
      timeClient.forceUpdate();
    }
    
    Firebase.pushString("history", String(timeClient.getEpochTime()) + " " + sensorName);
    
    Firebase.setString("alarm_state/alarm_state", "ringing");
    
    FirebaseObject firebaseObjectDevices = Firebase.get("devices");
    
    int i;
    
    for(i = 0; i < firebaseObjectDevices.getJsonVariant().as<JsonArray>().size(); i++) {
      Serial.println("Device id: " + firebaseObjectDevices.getJsonVariant().as<JsonArray>().get<String>(i));
      sendPushNotification(firebaseObjectDevices.getJsonVariant().as<JsonArray>().get<String>(i), sensorName);
    }
  }
}

void sendPushNotification(String phoneIdentifier, String sensorName) {
  WiFiClientSecure client;

  String url = "/fcm/send";
  const char host[] = "fcm.googleapis.com";
  const int httpsPort = 443;
  const char fingerprint[] = "";

  Serial.print("connecting to ");
  Serial.println(host);

  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  client.setFingerprint(fingerprint);

  if (!client.connect(host, 443)) {
    Serial.println("connection failed");
    return;
  }

  Serial.print("requesting URL: ");
  Serial.println(url);

  String data = "{" ;
  data = data + "\"to\": \"" + phoneIdentifier + "\"," ;
  data = data + "\"notification\": {" ;
  data = data + "\"body\": \"" + sensorName + "\"," ;
  data = data + "\"android_channel_id\": \"alarme\"," ;
  data = data + "\"icon\": \"alarm_icon\"," ;
  data = data + "\"sound\": \"alarm\"," ;
  data = data + "\"color\": \"#FF0000\"," ;
  data = data + "\"notification_priority\": \"PRIORITY_MAX\"," ;
  data = data + "\"title\" : \"Alarme de casa disparado\" " ;
  data = data + "} }" ;

  int contentLength = data.length();

  client.print("POST /fcm/send HTTP/1.1\nHost: fcm.googleapis.com\nAuthorization: key=\nConnection: close\nContent-type: application/json\naccept: */*\nContent-Length: " + String(contentLength) + "\n\n" + data);

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readString();
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
}