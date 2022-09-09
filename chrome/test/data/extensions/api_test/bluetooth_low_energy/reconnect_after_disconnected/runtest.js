// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress0 = '11:22:33:44:55:66';
var ble = chrome.bluetoothLowEnergy;

var errorAlreadyConnected = 'Already connected';

function expectError(message) {
  if (!chrome.runtime.lastError ||
      chrome.runtime.lastError.message != message)
    chrome.test.fail('Expected error: ' + message);
}

function expectSuccess() {
  if (chrome.runtime.lastError)
    chrome.test.fail('Unexpected error: ' + chrome.runtime.lastError.message);
}

// First attempt should succeed.
ble.connect(deviceAddress0, function () {
  expectSuccess();

  // Second attempt should fail.
  ble.connect(deviceAddress0, function () {
    expectError(errorAlreadyConnected);

    // Third attempt should succeed.
    ble.connect(deviceAddress0, function () {
      expectSuccess();
      chrome.test.succeed();
    });
  });
});
