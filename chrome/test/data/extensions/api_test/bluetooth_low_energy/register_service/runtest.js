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

var service1 = { uuid: '00001234-0000-1000-8000-00805f9b34fb', isPrimary: true }
var service2 = { uuid: '00001234-0000-1000-8000-00805f9b34fb', isPrimary: true }
chrome.bluetoothLowEnergy.createService(service1, function(serviceId) {
  if (failOnError(serviceId))
    return;
  chrome.bluetoothLowEnergy.registerService(serviceId, function() {
    if (failOnError('result'))
      return;
    // Registering again, this should fail.
    chrome.bluetoothLowEnergy.registerService(serviceId, function() {
      if (failOnSuccess())
        return;
      chrome.bluetoothLowEnergy.createService(service2, function(serviceId) {
        if (failOnError(serviceId))
          return;
        // Registering another service, this should work.
        chrome.bluetoothLowEnergy.registerService(serviceId, function() {
          if (failOnError('result'))
            return;
          chrome.test.succeed();
        });
      });
    });
  });
});
