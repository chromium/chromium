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

const service = {
  uuid: '00001234-0000-1000-8000-00805f9b34fb',
  isPrimary: true,
};
chrome.bluetoothLowEnergy.createService(service, function(serviceId) {
  if (failOnError(serviceId)) {
    return;
  }

  const characteristic1 = {
    uuid: '00001234-0000-1000-8000-00805f9b34fa',
    properties: ['notify'],
  };
  const characteristic2 = {
    uuid: '00001234-0000-1000-8000-00805f9b34fa',
    properties: ['indicate'],
  };
  chrome.bluetoothLowEnergy.createCharacteristic(
      characteristic1, serviceId, function(characteristicId1) {
        if (failOnError(characteristicId1)) {
          return;
        }
        chrome.bluetoothLowEnergy.createCharacteristic(
            characteristic2, serviceId, function(characteristicId2) {
              if (failOnError(characteristicId2)) {
                return;
              }

              const bytes = [0xBA, 0xAD, 0x72, 0xF0, 0x0D, 0x65];
              const newValue = (new Uint8Array(bytes)).buffer;
              // Notifying without registering first. Should fail.
              chrome.bluetoothLowEnergy.notifyCharacteristicValueChanged(
                  characteristicId1, {value: newValue, shouldIndicate: false},
                  function() {
                    if (failOnSuccess()) {
                      return;
                    }
                    chrome.bluetoothLowEnergy.registerService(
                        serviceId, function() {
                          if (failOnError('result')) {
                            return;
                          }
                          // Using notify with indicate property. Should fail.
                          chrome.bluetoothLowEnergy
                              .notifyCharacteristicValueChanged(
                                  characteristicId2,
                                  {value: newValue, shouldIndicate: false},
                                  function() {
                                    if (failOnSuccess()) {
                                      return;
                                    }
                                    // Using indicate with notify property.
                                    // Should fail.
                                    chrome.bluetoothLowEnergy
                                        .notifyCharacteristicValueChanged(
                                            characteristicId1, {
                                              value: newValue,
                                              shouldIndicate: true,
                                            },
                                            function() {
                                              if (failOnSuccess()) {
                                                return;
                                              }
                                              chrome.test.succeed();
                                            });
                                  });
                        });
                  });
            });
      });
});
