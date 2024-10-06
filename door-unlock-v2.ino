#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <FS.h>  // Include the SPIFFS library

const char* ssid = "NodeMCU"; //change name
const char* password = "12345678"; //change to strong password
const char* defaultPassword = "87654321"; //change to strong password

// For auto off
unsigned long lastOnTime = 0;
unsigned long currentTime = 0;
const int pinD1 = D1; 
ESP8266WebServer server(80);

// Function prototypes
void handleRoot();
void handleOn();
void handleSuccess();
void handleChangePassword();
void handleSetPassword();
void handleHistory();
String getStoredPassword();
void savePassword(String password);
void saveUnlockHistory(String timestamp);
String getUnlockHistory();

void setup() {
  Serial.begin(9600);
  
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
    return;
  }

  pinMode(pinD1, OUTPUT);
  digitalWrite(pinD1, LOW);
  WiFi.softAP(ssid, password);
  Serial.print("Access Point IP: ");
  Serial.println(WiFi.softAPIP());

  if (!SPIFFS.exists("/password.txt")) {
    savePassword(defaultPassword);
  }
  if (!SPIFFS.exists("/history.txt")) {
    File file = SPIFFS.open("/history.txt", "w");
    file.close();
  }

  server.on("/", handleRoot);
  server.on("/unlock", handleOn);
  server.on("/success", handleSuccess);
  server.on("/change-password-some-random-string", handleChangePassword); //change to secure url
  server.on("/set-password-random-string2", HTTP_POST, handleSetPassword); //change to secure url
  server.on("/history", handleHistory); 
  server.begin();
}

void loop() {
  server.handleClient();
  if (digitalRead(pinD1) == HIGH) {
    currentTime = millis();
    if (currentTime - lastOnTime >= 1000) {
      digitalWrite(pinD1, LOW);
    }
  }
}

void handleRoot() {
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body><center>";
  html += "<h4>Control Gate Lock</h4>";
  html += "<form action='/unlock' method='POST'>Password: <input type='password' name='password'><input type='text' name='ut' id='ut' hidden><br><br><input type='submit' value='Unlock'></form>";
  html += "</center></body><script>document.getElementById('ut').value = new Date().toLocaleString();</script></html>";
  server.send(200, "text/html", html);
}

void handleOn() {
  if (server.hasArg("password")) {
    String enteredPassword = server.arg("password");
    String storedPassword = getStoredPassword();

    if (enteredPassword == storedPassword) {
      digitalWrite(pinD1, HIGH);
      lastOnTime = millis();
      String timestamp = server.arg("ut");
      saveUnlockHistory(timestamp);
      server.sendHeader("Location", "/success");
      server.send(302, "text/plain", "");
    } else {
      server.send(401, "text/plain", "Unauthorized: Incorrect password");
    }
  } else {
    server.send(400, "text/plain", "Bad Request: Password not provided");
  }
}

void handleSuccess() {
  if (digitalRead(pinD1) == HIGH) {
    String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body><center>";
    html += "<h4>Gate unlocked on <span id='lastUpdate'> </span></h4>";
    html += "<a href='/'>Back</a>";
    html += "</center></body><script>let options24 = { year: 'numeric', month: '2-digit', day: '2-digit', hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: false }; document.getElementById('lastUpdate').innerText = new Date().toLocaleString('en-IN',options24);</script></html>";
    server.send(200, "text/html", html);   
  } else {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  }
}

void handleChangePassword() {
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'></head><body><center>";
  html += "<h4>Change Password</h4>";
  //change action url according to above set-password url
  html += "<form action='/set-password-random-string2' method='POST'>New Password: <input type='text' minlength='8' name='newPassword'><br><br><input type='submit' value='Set Password'></form></center></body></html>";
  server.send(200, "text/html", html);
}

void handleSetPassword() {
  if (server.hasArg("newPassword")) {
    String newPassword = server.arg("newPassword");
    savePassword(newPassword);
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  } else {
    server.send(400, "text/plain", "Bad Request: Password not provided");
  }
}

void handleHistory() {
  String history = getUnlockHistory();
  String html = "<!DOCTYPE html><html lang='en'><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><title>Unlock History</title></head><body><center>";
  html += "<a href='/'>Back</a><h4>Unlock History</h4>";
  html += "<table border='1' style='border-collapse: collapse;'><tr><th>SN</th><th>Timestamp</th></tr>";

  if (history.length() > 0) {
    html += history;
  } else {
    html += "<tr><td>No unlock records found.</td></tr>";
  }
  html += "</table></center></body></html>";
  server.send(200, "text/html", html);
}

void saveUnlockHistory(String timestamp) {
    // Open the history file for reading
    File file = SPIFFS.open("/history.txt", "r");
    if (!file) {
        Serial.println("Failed to open history file for reading");
        return;
    }

    // Read existing entries into a list
    String entries[100];
    int count = 0;

    while (file.available()) {
        entries[count] = file.readStringUntil('\n');
        count++;
        if (count >= 100) break; // Ensure we don't exceed the array size
    }
    file.close();

    // Open the history file for writing (this will clear the file)
    file = SPIFFS.open("/history.txt", "w");
    if (!file) {
        Serial.println("Failed to open history file for writing");
        return;
    }

    // Write back existing entries, discarding the oldest if there are 100
    for (int i = (count >= 100) ? 1 : 0; i < count; i++) {
        file.println(entries[i]);
    }

    // Add the new entry
    file.println(timestamp);

    file.close();
}

String getUnlockHistory() {
  File file = SPIFFS.open("/history.txt", "r");
  if (!file) {
    Serial.println("Failed to open history file");
    return "";
  }

  String history = "";
  String line;
  int count = 0;
  while (file.available()) {
    count++;
    line = file.readStringUntil('\n');
    history = "<tr><td>"+String(101-count)+"</td><td>" + line + "</td></tr>" + history;  // Store in reverse order
  }
  file.close();
  return history;
}

String getStoredPassword() {
  File file = SPIFFS.open("/password.txt", "r");
  if (!file) {
    Serial.println("Failed to open password file");
    return "";
  }
  String password = file.readString();
  file.close();
  return password;
}

void savePassword(String password) {
  File file = SPIFFS.open("/password.txt", "w");
  if (!file) {
    Serial.println("Failed to open password file for writing");
    return;
  }
  file.print(password);
  file.close();
}
