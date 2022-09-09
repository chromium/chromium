// Copyright 2016 The Chromium Authors
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

var service = { uuid: '00001234-0000-1000-8000-00805f9b34fb', isPrimary: true }
// Create then remove a service.
chrome.bluetoothLowEnergy.createService(service, function(serviceId) {
  if (failOnError(serviceId))
    return;
  chrome.bluetoothLowEnergy.removeService(serviceId, function() {
    if (failOnError('result'))
      return;
    // Registering a removed service. Should fail.
    chrome.bluetoothLowEnergy.registerService(serviceId, function() {
      if (failOnSuccess())
        return;

      // Create, register then remove a service.
      chrome.bluetoothLowEnergy.createService(service, function(serviceId) {
        if (failOnError(serviceId))
          return;
        chrome.bluetoothLowEnergy.registerService(serviceId, function() {
          if (failOnError('result'))
            return;
          chrome.bluetoothLowEnergy.removeService(serviceId, function() {
            if (failOnError('result'))
              return;
            // Removing an fake service. Should fail.
            chrome.bluetoothLowEnergy.removeService('fake_id', function() {
              if (failOnSuccess())
                return;
              chrome.test.succeed();
            });
          });
        });
      });
    });
  });
});
