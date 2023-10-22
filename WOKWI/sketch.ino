#include <WiFi.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <PubSubClient.h>

// Configurações de WiFi
const char *SSID = "Wokwi-GUEST";
const char *PASSWORD = "";  // Substitua pelo sua senha

// Configurações de MQTT
const char *BROKER_MQTT = "broker.hivemq.com";
const int BROKER_PORT = 1883;
const char *ID_MQTT = "lifesustent";
const char *TOPIC_SUBSCRIBE_LED = "fiap/lifesustent/led";
const char *TOPIC_PUBLISH_TEMP = "fiap/lifesustent/temp";
const char *TOPIC_PUBLISH_UMID = "fiap/lifesustent/umid";
const char *TOPIC_PUBLISH_ESTQ = "fiap/lifesustent/estq";

// Configurações de Hardware
#define PIN_DHT 12
#define PIN_RED 15
#define PIN_YELLOW 2
#define PIN_GREEN 4
#define PIN_ULTRASONIC_TRIG 14
#define PIN_ULTRASONIC_ECHO 27
#define PUBLISH_DELAY 2000

// Variáveis globais
WiFiClient espClient;
PubSubClient MQTT(espClient);
DHTesp dht;
unsigned long publishUpdate = 0;
TempAndHumidity sensorValues;
const int TAMANHO = 200;
float duration_us, distance_cm;
float estoque;

// Protótipos de funções
void updateSensorValues();
void initWiFi();
void initMQTT();
void callbackMQTT(char *topic, byte *payload, unsigned int length);
void reconnectMQTT();
void reconnectWiFi();
void checkWiFIAndMQTT();

void updateSensorValues() {
  sensorValues = dht.getTempAndHumidity();
}

void initWiFi() {
  Serial.print("Conectando com a rede: ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado com sucesso: ");
  Serial.println(SSID);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(callbackMQTT);
}

void callbackMQTT(char *topic, byte *payload, unsigned int length) {
  String msg = String((char*)payload).substring(0, length);
  
  Serial.printf("Mensagem recebida via MQTT: %s do tópico: %s\n", msg.c_str(), topic);

  if (strcmp(topic, TOPIC_SUBSCRIBE_LED) == 0) {
    int valor = atoi(msg.c_str());

    if (valor == 1) {
      digitalWrite(PIN_RED, HIGH);
      Serial.println("LED ligado via comando MQTT");
    } else if (valor == 0) {
      digitalWrite(PIN_RED, LOW);
      Serial.println("LED desligado via comando MQTT");
    }
  }
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("Tentando conectar com o Broker MQTT: ");
    Serial.println(BROKER_MQTT);

    if (MQTT.connect(ID_MQTT)) {
      Serial.println("Conectado ao broker MQTT!");
      MQTT.subscribe(TOPIC_SUBSCRIBE_LED);
    } else {
      Serial.println("Falha na conexão com MQTT. Tentando novamente em 2 segundos.");
      delay(2000);
    }
  }
}

void checkWiFIAndMQTT() {
  if (WiFi.status() != WL_CONNECTED) reconnectWiFi();
  if (!MQTT.connected()) reconnectMQTT();
}

void reconnectWiFi(void) {
  if (WiFi.status() == WL_CONNECTED)
    return;

  WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI

  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Wifi conectado com sucesso");
  Serial.print(SSID);
  Serial.println("IP: ");
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_ULTRASONIC_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONIC_ECHO, INPUT);
  digitalWrite(PIN_RED, LOW);
  dht.setup(PIN_DHT, DHTesp::DHT22);
  initWiFi();
  initMQTT();
}

void sendMessage(StaticJsonDocument<TAMANHO> doc, const char *TOPIC) {
  char buffer[TAMANHO];
  serializeJson(doc, buffer);
  MQTT.publish(TOPIC, buffer);
  Serial.println(buffer);
}

void loop() {
  checkWiFIAndMQTT();
  MQTT.loop();

  if ((millis() - publishUpdate) >= PUBLISH_DELAY) {
    publishUpdate = millis();
    updateSensorValues();

    digitalWrite(PIN_ULTRASONIC_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_TRIG, LOW);

    // measure duration of pulse from ECHO pin
    duration_us = pulseIn(PIN_ULTRASONIC_ECHO, HIGH);
  
    // calculate the distance
    distance_cm = 0.017 * duration_us;  

    estoque = distance_cm / 4;
  


    if (!isnan(sensorValues.temperature) && !isnan(sensorValues.humidity) && !isnan(estoque)) {
      

      
      StaticJsonDocument<TAMANHO> temp;
      StaticJsonDocument<TAMANHO> umid;
      StaticJsonDocument<TAMANHO> estq;
      temp["temperatura"] = sensorValues.temperature;
      umid["umidade"] = sensorValues.humidity;
      estq["estoque"] = estoque;

      if (estoque < 30) {
        estq["status"] = "Estoque muito baixo";
        digitalWrite(PIN_GREEN, LOW);
        digitalWrite(PIN_RED, HIGH);
      } else {
        digitalWrite(PIN_GREEN, HIGH);
        digitalWrite(PIN_RED, LOW);

      }


      bool exceedTemperatureLimit = sensorValues.temperature >= 10 && sensorValues.temperature <= 30;
      bool exceedUmidityLimit = sensorValues.humidity >= 15 && sensorValues.humidity <= 50;

      if (!exceedTemperatureLimit || !exceedUmidityLimit) {
        if (!exceedTemperatureLimit) {
          temp["status"] = "Problema de Temperatura";
        }
        if(!exceedUmidityLimit) {
          umid["status"] = "Problema de Umidade";
        }

        digitalWrite(PIN_YELLOW, HIGH);
      } else{
        digitalWrite(PIN_YELLOW, LOW);
      }

      sendMessage(temp, TOPIC_PUBLISH_TEMP);
      sendMessage(umid, TOPIC_PUBLISH_UMID);
      sendMessage(estq, TOPIC_PUBLISH_ESTQ);

    }
  }
}
