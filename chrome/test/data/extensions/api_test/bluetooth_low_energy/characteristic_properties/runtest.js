// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testCharacteristicProperties() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertEq(requestCount, characteristics.length);

  for (var i = 0; i < requestCount; i++) {
    compareProperties(expectedProperties[i], characteristics[i].properties);
  }

  chrome.test.succeed();
}

var charId = 'char_id0';
var requestCount = 12;
var characteristics = [];
var expectedProperties = [
  [],
  ['broadcast'],
  ['read'],
  ['writeWithoutResponse'],
  ['write'],
  ['notify'],
  ['indicate'],
  ['authenticatedSignedWrites'],
  ['extendedProperties'],
  ['reliableWrite'],
  ['writableAuxiliaries'],
  ['broadcast', 'read', 'writeWithoutResponse', 'write', 'notify', 'indicate',
   'authenticatedSignedWrites', 'extendedProperties', 'reliableWrite',
   'writableAuxiliaries']
];

function compareProperties(a, b) {
  chrome.test.assertEq(a.length, b.length);
  a.sort();
  b.sort();

  for (var i = 0; i < a.length; i++) {
    chrome.test.assertEq(a[i], b[i]);
  }
}

function failOnError() {
  if (error !== undefined)
    return true;

  if (chrome.runtime.lastError) {
    error = 'Unexpected error: ' + chrome.runtime.lastError.message;
    chrome.test.runTests([testCharacteristicProperties]);
  }
  return false;
}

for (var i = 0; i < requestCount; i++) {
  chrome.bluetoothLowEnergy.getCharacteristic(charId, function (result) {
    if (failOnError())
      return;

    characteristics.push(result);

    if (characteristics.length == requestCount) {
      chrome.test.sendMessage('ready', function (message) {
        chrome.test.runTests([testCharacteristicProperties]);
      });
    }
  });
}
