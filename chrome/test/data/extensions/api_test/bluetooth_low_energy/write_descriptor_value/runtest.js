// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testWriteDescriptorValue() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, descriptor, '\'descriptor\' is null');
  chrome.test.assertEq(descId, descriptor.instanceId);

  chrome.test.assertEq(writeValue.byteLength, descriptor.value.byteLength);

  var receivedValueBytes = new Uint8Array(descriptor.value);
  for (var i = 0; i < writeValue.byteLength; i++) {
    chrome.test.assertEq(valueBytes[i], receivedValueBytes[i]);
  }

  chrome.test.succeed();
}

function earlyError(message) {
  error = message;
  chrome.test.runTests([testWriteDescriptorValue]);
}

var writeDescriptorValue = chrome.bluetoothLowEnergy.writeDescriptorValue;
var descId = 'desc_id0';
var badDescId = 'desc_id1';

var descriptor = null;

var bytes = [0x43, 0x68, 0x72, 0x6F, 0x6D, 0x65];
var writeValue = new ArrayBuffer(bytes.length);
var valueBytes = new Uint8Array(writeValue);
valueBytes.set(bytes);

// 1. Unknown descriptor instanceId.
writeDescriptorValue(badDescId, writeValue, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('\'badDescId\' did not cause failure');
    return;
  }

  // 2. Known descriptor instanceId, but call failure.
  writeDescriptorValue(descId, writeValue, function (result) {
    if (result || !chrome.runtime.lastError) {
      earlyError('writeDescriptorValue should have failed');
      return;
    }

    // 3. Call should succeed.
    writeDescriptorValue(descId, writeValue, function (result) {
      if (chrome.runtime.lastError) {
        earlyError(chrome.runtime.lastError.message);
        return;
      }

      chrome.bluetoothLowEnergy.getDescriptor(descId, function (result) {
        descriptor = result;

        chrome.test.sendMessage('ready', function (message) {
          chrome.test.runTests([testWriteDescriptorValue]);
        });
      });
    });
  });
});
