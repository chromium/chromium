// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Triggers tab capture asynchronously. If successful, sends a "success"
// message.
function startTabCapture() {
  chrome.tabCapture.capture({audio: true, video: false}, captureStream => {
    if (chrome.runtime.lastError) {
      console.error(JSON.stringify(chrome.runtime.lastError));
    }
    if (captureStream) {
      console.log('Sending success response...');
      chrome.test.sendMessage("success");
    }
  });
}

// Loop that runs one iteration every time we get a response to the "ready"
// message.
function loop() {
  chrome.test.sendMessage("ready", () => {
    startTabCapture();
    loop();
  });
}

loop();

console.log('Extension loaded.');
