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
chrome.bluetoothLowEnergy.createService(service, function(serviceId) {
  if (failOnError(serviceId))
    return;

  chrome.bluetoothLowEnergy.registerService(serviceId, function() {
    if (failOnError('result'))
      return;

    chrome.bluetoothLowEnergy.unregisterService(serviceId, function() {
      if (failOnError('result'))
        return;

      // Unregistering an unregistered app, should fail.
      chrome.bluetoothLowEnergy.unregisterService('fake_id', function() {
        if (failOnSuccess())
          return;

        // Unregistering an unregistered service again, should fail.
        chrome.bluetoothLowEnergy.unregisterService(serviceId, function() {
          if (failOnSuccess())
            return;
          chrome.test.succeed();
        });
      });
    });
  });
});
