#include <WiFi.h>
#include <WebServer.h>
#include "DHT.h"

// ESP32 Wi-Fi: No mobile hotspot needed
const char* ap_ssid = "SmartClassroom";
const char* ap_password = "classroom123";

#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN 34
#define SOUND_PIN 35

#define SOUND_THRESHOLD 500
#define SMOKE_THRESHOLD 2000

DHT dht(DHTPIN, DHTTYPE);
WebServer server(80);

// =========================
// DASHBOARD HTML
// =========================
const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Smart Classroom</title>
<style>
body{font-family:Arial;background:#f2f2f2;text-align:center;margin:0}
.header{background:#222;color:white;padding:20px}
.container{display:flex;flex-wrap:wrap;justify-content:center;gap:20px;margin:25px 12px}
.card{background:white;width:220px;max-width:80%;padding:22px;border-radius:15px;box-shadow:0 4px 10px #ccc}
.value{font-size:32px;font-weight:bold}
.status{margin:20px auto;padding:18px;width:85%;border-radius:15px;background:#d9ffd9;color:green;font-size:20px;font-weight:bold}
.warning{background:#ffd6d6;color:#b00020}
.note{font-size:13px;color:#555;padding:10px}
</style>
</head>
<body>
<div class="header">
<h1>SMART CLASSROOM</h1>
<p>IoT Based Classroom Environment Monitoring System</p>
</div>

<div class="container">
<div class="card"><h2>🌡 Temperature</h2><div class="value"><span id="temperature">--</span> °C</div></div>
<div class="card"><h2>💧 Humidity</h2><div class="value"><span id="humidity">--</span> %</div></div>
<div class="card"><h2>💨 Smoke / Gas</h2><div class="value" id="smoke">--</div><p>Raw sensor reading</p></div>
<div class="card"><h2>🔊 Noise</h2><div class="value" id="noise">--</div><p>Relative sensor reading</p></div>
</div>

<div id="status" class="status">Waiting for sensor data...</div>
<p class="note">ESP32 local Wi-Fi dashboard. Internet not required. Sensor readings and thresholds need calibration.</p>

<script>
async function updateData(){
  try{
    const response = await fetch('/data');
    if(!response.ok) throw new Error('Connection error');
    const data = await response.json();

    document.getElementById('temperature').textContent =
      data.temperature === null ? 'Error' : data.temperature;
    document.getElementById('humidity').textContent =
      data.humidity === null ? 'Error' : data.humidity;
    document.getElementById('smoke').textContent = data.smoke;
    document.getElementById('noise').textContent = data.noise;

    const status = document.getElementById('status');
    if(data.smokeWarning || data.soundDetected){
      let messages = [];
      if(data.smokeWarning) messages.push('SMOKE/GAS LEVEL HIGH');
      if(data.soundDetected) messages.push('LOUD SOUND DETECTED');
      status.textContent = 'WARNING: ' + messages.join(' | ');
      status.classList.add('warning');
    }else{
      status.textContent = 'NO THRESHOLD ALERT';
      status.classList.remove('warning');
    }
  }catch(error){
    document.getElementById('status').textContent =
      'Connection error. Check Wi-Fi and refresh.';
    document.getElementById('status').classList.add('warning');
  }
}
setInterval(updateData, 1000);
updateData();
</script>
</body>
</html>
)rawliteral";

// =========================
// MAIN WEBPAGE
// =========================
void handleRoot() {
  server.send_P(200, "text/html", webpage);
}

// =========================
// SOUND SENSOR
// =========================
int readSoundLevel() {
  int soundMin = 4095;
  int soundMax = 0;

  for (int i = 0; i < 50; i++) {
    int value = analogRead(SOUND_PIN);

    if (value < soundMin) soundMin = value;
    if (value > soundMax) soundMax = value;

    delayMicroseconds(500);
  }

  return soundMax - soundMin;
}

// =========================
// SENSOR DATA API
// =========================
void handleData() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  int smokeValue = analogRead(MQ2_PIN);
  int soundValue = readSoundLevel();

  bool smokeWarning = smokeValue > SMOKE_THRESHOLD;
  bool soundDetected = soundValue > SOUND_THRESHOLD;

  // Serial Monitor readings
  Serial.printf(
    "Temperature: %.1f C | Humidity: %.1f %% | Smoke ADC: %d | Noise: %d\n",
    isnan(temperature) ? 0.0 : temperature,
    isnan(humidity) ? 0.0 : humidity,
    smokeValue,
    soundValue
  );

  // Create JSON response
  String json = "{";

  json += "\"temperature\":";
  json += isnan(temperature) ? "null" : String(temperature, 1);

  json += ",\"humidity\":";
  json += isnan(humidity) ? "null" : String(humidity, 1);

  json += ",\"smoke\":";
  json += String(smokeValue);

  json += ",\"noise\":";
  json += String(soundValue);

  json += ",\"smokeWarning\":";
  json += smokeWarning ? "true" : "false";

  json += ",\"soundDetected\":";
  json += soundDetected ? "true" : "false";

  json += "}";

  server.send(200, "application/json", json);
}

// =========================
// SETUP
// =========================
void setup() {
  Serial.begin(115200);
  delay(500);

  dht.begin();

  pinMode(MQ2_PIN, INPUT);
  pinMode(SOUND_PIN, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(MQ2_PIN, ADC_11db);
  analogSetPinAttenuation(SOUND_PIN, ADC_11db);

  Serial.println();
  Serial.println("==============================");
  Serial.println(" SMART CLASSROOM SYSTEM");
  Serial.println("==============================");

  // ESP32 creates its own Wi-Fi network
  WiFi.mode(WIFI_AP);

  if (WiFi.softAP(ap_ssid, ap_password)) {
    Serial.println("ESP32 Wi-Fi Started!");

    Serial.print("Wi-Fi Name: ");
    Serial.println(ap_ssid);

    Serial.print("Wi-Fi Password: ");
    Serial.println(ap_password);

    Serial.print("Dashboard IP: ");
    Serial.println(WiFi.softAPIP());

    server.on("/", handleRoot);
    server.on("/data", handleData);

    server.begin();

    Serial.println("Web Server Started!");
    Serial.println("Open http://192.168.4.1");
  } else {
    Serial.println("Wi-Fi start failed. Restart ESP32.");
  }
}

// =========================
// LOOP
// =========================
void loop() {
  server.handleClient();
}
