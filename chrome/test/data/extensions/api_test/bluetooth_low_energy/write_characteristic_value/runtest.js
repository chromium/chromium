// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testWriteCharacteristicValue() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, characteristic, '\'characteristic\' is null');
  chrome.test.assertEq(charId, characteristic.instanceId);

  chrome.test.assertEq(writeValue.byteLength, characteristic.value.byteLength);

  var receivedValueBytes = new Uint8Array(characteristic.value);
  for (var i = 0; i < writeValue.byteLength; i++) {
    chrome.test.assertEq(valueBytes[i], receivedValueBytes[i]);
  }

  chrome.test.succeed();
}

var writeCharacteristicValue =
    chrome.bluetoothLowEnergy.writeCharacteristicValue;

var charId = 'char_id0';
var badCharId = 'char_id1';

var characteristic = null;

var bytes = [0x43, 0x68, 0x72, 0x6F, 0x6D, 0x65];
var writeValue = new ArrayBuffer(bytes.length);
var valueBytes = new Uint8Array(writeValue);
valueBytes.set(bytes);

function earlyError(message) {
  error = message;
  chrome.test.runTests([testWriteCharacteristicValue]);
}

// 1. Unknown characteristic instanceId.
writeCharacteristicValue(badCharId, writeValue, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('\'badCharId\' did not cause failure');
    return;
  }

  // 2. Known characteristic instanceId, but call failure.
  writeCharacteristicValue(charId, writeValue, function (result) {
    if (result || !chrome.runtime.lastError) {
      earlyError('writeCharacteristicValue should have failed');
      return;
    }

    // 3. Call should succeed.
    writeCharacteristicValue(charId, writeValue, function (result) {
      if (chrome.runtime.lastError) {
        earlyError(chrome.runtime.lastError.message);
        return;
      }

      chrome.bluetoothLowEnergy.getCharacteristic(charId, function (result) {
        characteristic = result;

        chrome.test.sendMessage('ready', function (message) {
          chrome.test.runTests([testWriteCharacteristicValue]);
        });
      });
    });
  });
});
