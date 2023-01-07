// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGetServices() {
  chrome.test.assertEq(2, services.length);

  chrome.test.assertEq('service_id0', services[0].instanceId);
  chrome.test.assertEq('00001234-0000-1000-8000-00805f9b34fb',
                       services[0].uuid);
  chrome.test.assertEq(true , services[0].isPrimary);
  chrome.test.assertEq(deviceAddress, services[0].deviceAddress);

  chrome.test.assertEq('service_id1', services[1].instanceId);
  chrome.test.assertEq('00005678-0000-1000-8000-00805f9b34fb',
                       services[1].uuid);
  chrome.test.assertEq(false , services[1].isPrimary);
  chrome.test.assertEq(deviceAddress, services[1].deviceAddress);

  chrome.test.succeed();
}

var deviceAddress = '11:22:33:44:55:66';
var serviceId = 'service_id0';
var charId = 'char_id0';
var descId = 'char_id0';

function checkError() {
  if (!chrome.runtime.lastError) {
    chrome.test.fail("Expected an error");
  }
  chrome.test.assertEq("Permission denied", chrome.runtime.lastError.message);
}

chrome.bluetoothLowEnergy.getServices(deviceAddress, function(result) {
  checkError();
  chrome.bluetoothLowEnergy.getService(serviceId, function(result) {
    checkError();
    chrome.bluetoothLowEnergy.getCharacteristics(serviceId, function(result) {
      checkError();
      chrome.bluetoothLowEnergy.getCharacteristic(charId, function(result) {
        checkError();
        chrome.bluetoothLowEnergy.getDescriptors(charId, function(result) {
          checkError();
          chrome.bluetoothLowEnergy.getDescriptor(descId, function(result) {
            checkError();
            chrome.test.succeed();
          });
        });
      });
    });
  });
});
