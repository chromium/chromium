// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kExpectedDeviceNames = ["d1"];

function testDiscovery() {
  chrome.test.assertEq(kExpectedDeviceNames.length,
      discoveredDevices.length);
  for (var i = 0; i < kExpectedDeviceNames.length; ++i) {
    chrome.test.assertEq(kExpectedDeviceNames[i],
        discoveredDevices[i].name);
  }

  chrome.test.succeed();
}

function startTests() {
  chrome.test.runTests([testDiscovery]);
}

function sendReady(callback) {
  chrome.test.sendMessage('ready', callback);
}

var discoveredDevices = [];
function recordDevice(device) {
  discoveredDevices.push(device);
}

function stopDiscoveryAndContinue() {
  chrome.bluetooth.stopDiscovery();
  chrome.bluetooth.onDeviceAdded.removeListener(recordDevice);
  sendReady(startTests);
}

chrome.bluetooth.onDeviceAdded.addListener(recordDevice);
chrome.bluetooth.startDiscovery(
    function() { sendReady(stopDiscoveryAndContinue); });
