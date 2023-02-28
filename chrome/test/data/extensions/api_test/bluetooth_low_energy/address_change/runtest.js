// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testAddressChanged() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, service);

  chrome.test.assertEq(serviceId, service.instanceId);

  // Service was created for device with different address, check wether
  // it was updated to reflect that change.
  chrome.test.assertEq(newDeviceAddress, service.deviceAddress);

  chrome.test.succeed();
}

var newDeviceAddress = '11:22:33:44:55:77';
var serviceId = 'service_id0';

var service = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testAddressChanged]);
}

function failOnError() {
  if (chrome.runtime.lastError) {
    earlyError(chrome.runtime.lastError.message);
    return true;
  }
  return false;
}

chrome.bluetoothLowEnergy.getService(serviceId, function(result) {
  if (failOnError())
    return;

  service = result;

  chrome.test.sendMessage('ready', function(message) {
    chrome.test.runTests([testAddressChanged]);
  });
});
