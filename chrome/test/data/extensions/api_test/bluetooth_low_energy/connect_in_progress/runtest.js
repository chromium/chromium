// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress0 = '11:22:33:44:55:66';
var ble = chrome.bluetoothLowEnergy;

var errorInProgress = 'In progress';
var errorNotConnected = 'Not connected';

function expectError(message) {
  if (!chrome.runtime.lastError ||
      chrome.runtime.lastError.message != message)
    chrome.test.sendMessage('Expected error: <' + message + '> got <'
        + chrome.runtime.lastError.message + '>');
}

function expectSuccess() {
  if (chrome.runtime.lastError)
    chrome.test.sendMessage('Unexpected error: '
        + chrome.runtime.lastError.message);
}

ble.connect(deviceAddress0, function () {
  expectSuccess();
  ble.disconnect(deviceAddress0, function () {
    chrome.test.succeed();
  });

  ble.disconnect(deviceAddress0, function () {
    expectError(errorNotConnected);
    chrome.test.sendMessage(
        'After 2nd call to disconnect.');
  });
});

ble.connect(deviceAddress0, function () {
  expectError(errorInProgress);
  chrome.test.sendMessage(
      'After 2nd connect fails due to 1st connect being in progress.');
});
