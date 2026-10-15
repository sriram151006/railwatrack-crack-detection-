#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>
#include <EEPROM.h>

// ================= WIFI =================
const char* ssid = "sriram";
const char* password = "12345678";

// ================= EEPROM =================
#define EEPROM_SIZE 128
#define CAM_IP_ADDR 0     // starting address for camera IP string

String camIP = "192.168.1.100";  // default fallback value

WebServer server(80);

// ================= LCD =================
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= GPS =================
TinyGPSPlus gps;
HardwareSerial GPSserial(1);

// ================= MOTOR (L298N) =================
#define ENA 25
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12
#define ENB 33

int motorSpeed = 255; // 0-255 PWM speed

// ================= ULTRASONIC =================
#define TRIG 4
#define ECHO 2

// ================= BUZZER =================
#define BUZZER 15

long distanceCM;

// ================= STATUS =================
String alertMsg = "Track Safe";
String latStr = "11.0279602";
String lonStr = "77.0287641";

// ================= MOTOR FUNCTIONS =================
void setMotorSpeed(int speed) {
  analogWrite(ENA, speed);
  analogWrite(ENB, speed);
}

void stopMotor() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

void forward() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  setMotorSpeed(motorSpeed);
}

void backward() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  setMotorSpeed(motorSpeed);
}

void left() {
  digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
  setMotorSpeed(motorSpeed);
}

void right() {
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  setMotorSpeed(motorSpeed);
}

// ================= ULTRASONIC =================
long readDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);
  long duration = pulseIn(ECHO, HIGH, 30000);
  return duration * 0.034 / 2;
}

// ================= SAVE / LOAD CAMERA IP =================
void loadCamIP() {
  camIP = "";
  for (int i = 0; i < 20; i++) {  // max length ~15-20 chars
    char c = EEPROM.read(CAM_IP_ADDR + i);
    if (c == 0 || c == 255) break;
    camIP += c;
  }

  // If invalid or empty → use default
  if (camIP.length() < 7 || camIP.indexOf('.') == -1) {
    camIP = "192.168.1.100";  // change this to your usual default if needed
  }
}

void saveCamIP(String newIP) {
  newIP.trim();
  camIP = newIP;

  // Clear old data
  for (int i = 0; i < 32; i++) {
    EEPROM.write(CAM_IP_ADDR + i, 0);
  }

  // Write new IP
  for (int i = 0; i < camIP.length(); i++) {
    EEPROM.write(CAM_IP_ADDR + i, camIP[i]);
  }
  EEPROM.write(CAM_IP_ADDR + camIP.length(), 0); // null terminate
  EEPROM.commit();
}

// ================= WEB PAGE =================
void handleRoot() {
  String page;
  page.reserve(5000);

  page += "<!DOCTYPE html><html><head>";
  page += "<title>ESP32 Robot Monitor</title>";
  page += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";

  page += "<style>";
  page += "body{background:#111;color:white;text-align:center;font-family:Arial;}";
  page += "button{padding:12px 18px;font-size:16px;margin:6px;border-radius:8px;border:none;cursor:pointer;}";
  page += ".fwd{background:#00cc66;color:black;}";
  page += ".stp{background:red;color:white;}";
  page += ".turn{background:#0099ff;color:white;}";
  page += ".save{background:#ffaa00;color:black;}";
  page += ".card{border:2px solid #00ffaa;border-radius:12px;padding:16px;margin:16px;}";
  page += "input[type=text]{padding:10px;font-size:16px;width:220px;border-radius:6px;border:1px solid #444;}";
  page += "</style></head><body>";

  // ===== CAMERA =====
  page += "<h2>ESP32-CAM Live Stream</h2>";
  page += "<img id='cam' src='http://" + camIP + "/' width='320' onerror=\"this.src='';\"><br><br>";
  page += "<button onclick='reloadCam()'>RELOAD</button> ";
  page += "<button onclick='snapshot()'>SNAPSHOT</button> ";
  page += "<button onclick='fullscreen()'>FULLSCREEN</button><br><br>";

  // ===== CAMERA IP SETTINGS =====
  page += "<div class='card'><h3>Camera IP Settings</h3>";
  page += "<p>Current IP: <b>" + camIP + "</b></p>";
  page += "<input type='text' id='camip' placeholder='e.g. 192.168.1.100' value='" + camIP + "'>";
  page += "<br><br>";
  page += "<button class='save' onclick='saveCamIP()'>SAVE CAMERA IP</button>";
  page += "</div>";
c:\Users\user\Desktop\SMC\rail_way_track_esp_32
  // ===== MOTOR CONTROL =====
  page += "<div class='card'><h3>Motor Control</h3>";
  page += "<button class='fwd' onclick=\"cmd('forward')\">FORWARD</button><br>";
  page += "<button class='turn' onclick=\"cmd('left')\">LEFT</button>";
  page += "<button class='stp' onclick=\"cmd('stop')\">STOP</button>";
  page += "<button class='turn' onclick=\"cmd('right')\">RIGHT</button><br>";
  page += "<button class='fwd' onclick=\"cmd('backward')\">BACKWARD</button>";
  page += "</div>";

  // ===== ALERT =====
  page += "<div class='card'><h3>ALERT</h3>";
  page += alertMsg;
  page += "</div>";

  // ===== GPS =====
  page += "<div class='card'><h3>GPS Location</h3>";
  page += "Latitude: " + latStr + "<br>";
  page += "Longitude: " + lonStr + "<br><br>";
  page += "<button onclick='openMap()'>OPEN MAP</button>";
  page += "</div>";

  // ===== SCRIPT =====
  page += "<script>";
  page += "function cmd(c){fetch('/'+c);}";
  
  page += "function reloadCam(){";
  page += "  var ts = new Date().getTime();";
  page += "  document.getElementById('cam').src = 'http://" + camIP + "/?t=' + ts;";
  page += "}";

  page += "function snapshot(){";
  page += "  window.open('http://" + camIP + "/capture','_blank');}";
  
  page += "function fullscreen(){";
  page += "  window.open('http://" + camIP + "/','_blank');}";

  page += "function openMap(){";
  page += "  window.open('https://www.google.com/maps?q=" + latStr + "," + lonStr + "','_blank');}";

  page += "function saveCamIP(){";
  page += "  var ip = document.getElementById('camip').value.trim();";
  page += "  if(!ip || ip.length < 7 || ip.indexOf('.') === -1){";
  page += "    alert('Please enter a valid IP address'); return;";
  page += "  }";
  page += "  fetch('/setcamip?ip=' + encodeURIComponent(ip))";
  page += "    .then(r => r.text())";
  page += "    .then(txt => { alert(txt); location.reload(); })";
  page += "    .catch(() => alert('Error saving IP'));";
  page += "}";
  page += "</script>";

  page += "</body></html>";

  server.send(200, "text/html", page);
}

// ================= ROUTES =================
void handleForward()  { forward();  server.send(200, "text/plain", "FORWARD"); }
void handleBackward() { backward(); server.send(200, "text/plain", "BACKWARD"); }
void handleLeft()     { left();     server.send(200, "text/plain", "LEFT"); }
void handleRight()    { right();    server.send(200, "text/plain", "RIGHT"); }
void handleStop()     { stopMotor(); server.send(200, "text/plain", "STOP"); }

void handleSetCamIP() {
  if (server.hasArg("ip")) {
    String newIP = server.arg("ip");
    newIP.trim();

    if (newIP.length() >= 7 && newIP.indexOf('.') > 0) {
      saveCamIP(newIP);
      server.send(200, "text/plain", "Camera IP updated to: " + camIP);
    } else {
      server.send(400, "text/plain", "Invalid IP address");
    }
  } else {
    server.send(400, "text/plain", "Missing 'ip' parameter");
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  setMotorSpeed(motorSpeed);

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);

  lcd.init();
  lcd.backlight();

  GPSserial.begin(9600, SERIAL_8N1, 16, 17);

  EEPROM.begin(EEPROM_SIZE);
  loadCamIP();

  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("IP ADDRESS");
  lcd.setCursor(0,1);
  String ip = WiFi.localIP().toString();
  lcd.print(ip.substring(0, 16));

  Serial.println("ESP32 IP: " + ip);
  Serial.println("Camera IP: " + camIP);

  server.on("/", handleRoot);
  server.on("/forward", handleForward);
  server.on("/backward", handleBackward);
  server.on("/left", handleLeft);
  server.on("/right", handleRight);
  server.on("/stop", handleStop);
  server.on("/setcamip", handleSetCamIP);

  server.begin();
}

// ================= LOOP =================
void loop() {
  server.handleClient();

  distanceCM = readDistance();

  if (distanceCM > 3 && distanceCM < 1000) {   // added upper limit to avoid noise
    alertMsg = "CRACK DETECTED!";
    stopMotor();
    digitalWrite(BUZZER, HIGH);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ALERT:");
    lcd.setCursor(0,1);
    lcd.print("Track Gap");
  } 
  else {
    alertMsg = "Track Safe";
    digitalWrite(BUZZER, LOW);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("IP ADDRESS");
    lcd.setCursor(0,1);
    String ip = WiFi.localIP().toString();
    lcd.print(ip.substring(0,16));
  }

  // GPS Update
  while (GPSserial.available()) {
    gps.encode(GPSserial.read());
    if (gps.location.isUpdated()) {
      latStr = String(gps.location.lat(), 6);
      lonStr = String(gps.location.lng(), 6);
    }
  }
}