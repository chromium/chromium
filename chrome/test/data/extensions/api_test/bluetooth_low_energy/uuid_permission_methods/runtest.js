// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var deviceAddress = '11:22:33:44:55:66';
var serviceId = 'service_id0';
var charId = 'char_id0';
var descId = 'desc_id0';
var writeVal = new ArrayBuffer(0);

function checkError() {
  if (!chrome.runtime.lastError) {
    chrome.test.fail('Expected an error');
  }
  chrome.test.assertEq('Permission denied', chrome.runtime.lastError.message);
}

var ble = chrome.bluetoothLowEnergy;
ble.getServices(deviceAddress, function (result) {
  if (chrome.runtime.lastError) {
    chrome.test.fail('Unexpected error: ' + chrome.runtime.lastError.message);
    return;
  }
  chrome.test.assertEq(1, result.length);
  chrome.test.assertEq(serviceId, result[0].instanceId);

  ble.getService(serviceId, function (result) {
    if (chrome.runtime.lastError) {
      chrome.test.fail('Unexpected error: ' + chrome.runtime.lastError.message);
      return;
    }

    chrome.test.assertEq(serviceId, result.instanceId);

    ble.getCharacteristics(serviceId, function (result) {
      checkError();
      ble.getCharacteristic(charId, function (result) {
        checkError();
        ble.getDescriptors(charId, function (result) {
          checkError();
          ble.getDescriptor(descId, function (result) {
            checkError();
            ble.readCharacteristicValue(charId, function (result) {
              checkError();
              ble.writeCharacteristicValue(charId, writeVal, function (result) {
                checkError();
                ble.readDescriptorValue(descId, function (result) {
                  checkError();
                  ble.writeDescriptorValue(descId, writeVal, function (result) {
                    checkError();
                    chrome.test.succeed();
                  });
                });
              });
            });
          });
        });
      });
    });
  });
});
