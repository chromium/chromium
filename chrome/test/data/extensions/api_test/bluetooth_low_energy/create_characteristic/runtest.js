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
    properties: ['read']};
  // Invalid service ID.
  chrome.bluetoothLowEnergy.createCharacteristic(characteristic,
      'invalidServiceId', function(characteristicId) {
    if (failOnSuccess())
      return;

    // Valid service ID.
    chrome.bluetoothLowEnergy.createCharacteristic(characteristic, serviceId,
          function(characteristicId) {
        if (failOnError(characteristicId))
          return;
        chrome.test.succeed();
    });
  });
});
