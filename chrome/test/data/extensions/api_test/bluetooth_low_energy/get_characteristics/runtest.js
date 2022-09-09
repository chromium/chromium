// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testGetCharacteristics() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertEq(2, chrcs.length);

  chrome.test.assertEq('char_id0', chrcs[0].instanceId);
  chrome.test.assertEq('00001211-0000-1000-8000-00805f9b34fb', chrcs[0].uuid);
  chrome.test.assertEq(serviceId, chrcs[0].service.instanceId);
  chrome.test.assertEq(4, chrcs[0].properties.length);
  chrome.test.assertTrue(chrcs[0].properties.indexOf('broadcast') > -1,
                         '\'broadcast\' not in chrcs[0].properties');
  chrome.test.assertTrue(chrcs[0].properties.indexOf('read') > -1,
                         '\'read\' not in chrcs[0].properties');
  chrome.test.assertTrue(chrcs[0].properties.indexOf('indicate') > -1,
                         '\'indicate\' not in chrcs[0].properties');
  chrome.test.assertTrue(
      chrcs[0].properties.indexOf('writeWithoutResponse') > -1,
      '\'writeWithoutResponse\' not in chrcs[0].properties');

  var valueBytes = new Uint8Array(chrcs[0].value);
  chrome.test.assertEq(5, chrcs[0].value.byteLength);
  chrome.test.assertEq(0x01, valueBytes[0]);
  chrome.test.assertEq(0x02, valueBytes[1]);
  chrome.test.assertEq(0x03, valueBytes[2]);
  chrome.test.assertEq(0x04, valueBytes[3]);
  chrome.test.assertEq(0x05, valueBytes[4]);

  chrome.test.assertEq('char_id1', chrcs[1].instanceId),
  chrome.test.assertEq('00001212-0000-1000-8000-00805f9b34fb', chrcs[1].uuid);
  chrome.test.assertEq(serviceId, chrcs[1].service.instanceId);
  chrome.test.assertEq(3, chrcs[1].properties.length);
  chrome.test.assertTrue(chrcs[1].properties.indexOf('read') > -1,
                         '\'read\' not in chrcs[1].properties');
  chrome.test.assertTrue(chrcs[1].properties.indexOf('write') > -1,
                         '\'write\' not in chrcs[1].properties');
  chrome.test.assertTrue(chrcs[1].properties.indexOf('notify') > -1,
                         '\'notify\' not in chrcs[1].properties');

  valueBytes = new Uint8Array(chrcs[1].value);
  chrome.test.assertEq(3, chrcs[1].value.byteLength);
  chrome.test.assertEq(0x06, valueBytes[0]);
  chrome.test.assertEq(0x07, valueBytes[1]);
  chrome.test.assertEq(0x08, valueBytes[2]);

  chrome.test.succeed();
}

var serviceId = 'service_id0';
var chrcs = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testGetCharacteristics]);
}

function expectSuccess() {
  if (chrome.runtime.lastError) {
    earlyError('Unexpected Error: ' + chrome.runtime.lastError.message);
  }
  return error !== undefined;
}

chrome.bluetoothLowEnergy.getCharacteristics(serviceId, function (result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('getCharacteristics should have failed.');
    return;
  }

  chrome.bluetoothLowEnergy.getCharacteristics(serviceId, function (result) {
    if (expectSuccess())
      return;

    if (!result || result.length != 0) {
      earlyError('Characteristics should be empty.');
      return;
    }

    chrome.bluetoothLowEnergy.getCharacteristics(serviceId, function (result) {
      if (expectSuccess())
        return;

      chrcs = result;

      chrome.test.sendMessage('ready', function (message) {
        chrome.test.runTests([testGetCharacteristics]);
      });
    });
  });
});
