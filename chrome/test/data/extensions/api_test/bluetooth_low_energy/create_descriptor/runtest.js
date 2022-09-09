// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function failOnError(result) {
  if (chrome.runtime.lastError || !result) {
    chrome.test.fail(chrome.runtime.lastError.message);
    return true;
  }
  return false;
}

function failOnSuccess() {
  if (!chrome.runtime.lastError) {
    chrome.test.fail('lastError not set, operation succeeded.');
    return true;
  }
  return false;
}

var service = { uuid: '00001234-0000-1000-8000-00805f9b34fb', isPrimary: true };
chrome.bluetoothLowEnergy.createService(service, function(serviceId) {
  if (failOnError(serviceId))
    return;

  var characteristic = { uuid: '00001234-0000-1000-8000-00805f9b34fa',
    properties: ['read'] };
  chrome.bluetoothLowEnergy.createCharacteristic(characteristic, serviceId,
      function(characteristicId) {
    if (failOnError(characteristicId))
      return;

    var descriptor = { uuid: '00001234-0000-1000-8000-00805f9b34fc',
      permissions: ['read'] };
    // Invalid characteristic ID.
    chrome.bluetoothLowEnergy.createDescriptor(descriptor,
        'invalidCharacteristicId', function(descriptorId) {
      if (failOnSuccess())
        return;

      // Valid characteristic ID.
      chrome.bluetoothLowEnergy.createDescriptor(descriptor, characteristicId,
          function(descriptorId) {
        if (failOnError(descriptorId))
          return;

        chrome.test.succeed();
      });
    });
  });
});
