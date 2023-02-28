// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testGetService() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, service);

  chrome.test.assertEq(serviceId, service.instanceId);
  chrome.test.assertEq('00001234-0000-1000-8000-00805f9b34fb', service.uuid);
  chrome.test.assertEq(true , service.isPrimary);
  chrome.test.assertEq(deviceAddress, service.deviceAddress);

  chrome.test.succeed();
}

var deviceAddress = '11:22:33:44:55:66';
var serviceId = 'service_id0';
var badServiceId = 'service_id1';

var service = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testGetService]);
}

function failOnError() {
  if (chrome.runtime.lastError) {
    earlyError(chrome.runtime.lastError.message);
    return true;
  }
  return false;
}

function failOnSuccess(result) {
  if (result || !chrome.runtime.lastError) {
    earlyError('Unexpected service.');
    return true;
  }
  return false;
}

// 1. Unknown service instanceId.
chrome.bluetoothLowEnergy.getService(badServiceId, function(result) {
  if (failOnSuccess(result))
    return;

  // 2. Known service instanceId, but the mapped device is unknown.
  chrome.bluetoothLowEnergy.getService(serviceId, function(result) {
    if (failOnSuccess(result))
      return;

    // 3. Known service instanceId, but the mapped device does not know about
    // the service.
    chrome.bluetoothLowEnergy.getService(serviceId, function(result) {
      if (failOnSuccess(result))
        return;

      // 4. Success.
      chrome.bluetoothLowEnergy.getService(serviceId, function(result) {
        if (failOnError())
          return;

        service = result;

        chrome.test.sendMessage('ready', function(message) {
          chrome.test.runTests([testGetService]);
        });
      });
    });
  });
});
