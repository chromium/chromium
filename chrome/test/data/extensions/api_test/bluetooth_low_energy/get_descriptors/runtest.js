// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testGetDescriptors() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertEq(2, descrs.length);

  chrome.test.assertEq('desc_id0', descrs[0].instanceId);
  chrome.test.assertEq('00001221-0000-1000-8000-00805f9b34fb', descrs[0].uuid);
  chrome.test.assertEq(charId, descrs[0].characteristic.instanceId);

  var valueBytes = new Uint8Array(descrs[0].value);
  chrome.test.assertEq(3, descrs[0].value.byteLength);
  chrome.test.assertEq(0x01, valueBytes[0]);
  chrome.test.assertEq(0x02, valueBytes[1]);
  chrome.test.assertEq(0x03, valueBytes[2]);

  chrome.test.assertEq('desc_id1', descrs[1].instanceId);
  chrome.test.assertEq('00001222-0000-1000-8000-00805f9b34fb', descrs[1].uuid);
  chrome.test.assertEq(charId, descrs[1].characteristic.instanceId);

  valueBytes = new Uint8Array(descrs[1].value);
  chrome.test.assertEq(2, descrs[1].value.byteLength);
  chrome.test.assertEq(0x04, valueBytes[0]);
  chrome.test.assertEq(0x05, valueBytes[1]);

  chrome.test.succeed();
}

var getDescriptors = chrome.bluetoothLowEnergy.getDescriptors;
var charId = 'char_id0';
var badCharId = 'char_id1';

var descrs = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testGetDescriptors]);
}

function failOnError() {
  if (chrome.runtime.lastError) {
    earlyError(chrome.runtime.lastError.message);
    return true;
  }
  return false;
}

// 1. Unknown characteristic ID.
getDescriptors(badCharId, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('getDescriptors should have failed for \'badCharId\'');
    return;
  }

  // 2. Known ID, unknown characteristic.
  getDescriptors(charId, function (result) {
    if (result || !chrome.runtime.lastError) {
      earlyError('getDescriptors should have failed');
      return;
    }

    // 3. Empty descriptors.
    getDescriptors(charId, function (result) {
      if (failOnError())
        return;

      if (!result || result.length != 0) {
        earlyError('Descriptors should be empty');
        return;
      }

      // 4. Success.
      getDescriptors(charId, function (result) {
        if (failOnError())
          return;

        descrs = result;

        chrome.test.sendMessage('ready', function (message) {
          chrome.test.runTests([testGetDescriptors]);
        });
      });
    });
  });
});
