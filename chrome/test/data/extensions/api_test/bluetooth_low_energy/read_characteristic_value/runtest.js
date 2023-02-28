// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testReadCharacteristicValue() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, characteristic, '\'characteristic\' is null');
  chrome.test.assertEq(charId, characteristic.instanceId);

  chrome.test.succeed();
}

var readCharacteristicValue = chrome.bluetoothLowEnergy.readCharacteristicValue;
var charId = 'char_id0';
var badCharId = 'char_id1';

var characteristic = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testReadCharacteristicValue]);
}


// 1. Unknown characteristic instanceId.
readCharacteristicValue(badCharId, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('\'badCharId\' did not cause failure');
    return;
  }

  // 2. Known characteristic instanceId, but call failure.
  readCharacteristicValue(charId, function (result) {
    if (result || !chrome.runtime.lastError) {
      earlyError('readCharacteristicValue should have failed');
      return;
    }

    // 3. Call should succeed.
    readCharacteristicValue(charId, function (result) {
      if (chrome.runtime.lastError) {
        earlyError(chrome.runtime.lastError.message);
        return;
      }

      characteristic = result;

      chrome.test.sendMessage('ready', function (reply) {
        chrome.test.runTests([testReadCharacteristicValue]);
      });
    });
  });
});

