// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kExpectedDeviceNames = {
  "21:22:23:24:25:26": "the real d2",
  "31:32:33:34:35:36": "d3"
};

function testDeviceEvents() {
  var expectedAddresses = Object.keys(kExpectedDeviceNames);
  chrome.test.assertEq(expectedAddresses.length, Object.keys(devices).length);

  for (var i = 0; i < expectedAddresses.length; ++i) {
    var address = expectedAddresses[i];
    chrome.test.assertTrue(address in devices);
    chrome.test.assertEq(kExpectedDeviceNames[address], devices[address].name);
  }

  chrome.test.succeed();
}

function startTests() {
  chrome.test.runTests([testDeviceEvents]);
}

var devices = {};
function recordDevice(device) {
  devices[device.address] = device;
}
function removeDevice(device) {
  delete devices[device.address];
  chrome.test.sendMessage('ready', startTests);
}

function stopDiscoveryAndContinue() {
  chrome.bluetooth.stopDiscovery();
  chrome.bluetooth.onDeviceAdded.removeListener(recordDevice);
  sendReady(startTests);
}

chrome.bluetooth.onDeviceAdded.addListener(recordDevice);
chrome.bluetooth.onDeviceChanged.addListener(recordDevice);
chrome.bluetooth.onDeviceRemoved.addListener(removeDevice);
