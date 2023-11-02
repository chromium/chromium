// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var registerAdvertisement =
    chrome.bluetoothLowEnergy.registerAdvertisement;
var unregisterAdvertisement =
    chrome.bluetoothLowEnergy.unregisterAdvertisement;

var serviceUuidsValue = ['value1', 'value2'];
var manufacturerDataValue = [{id: 321, data: [1, 2, 3]},
                             {id: 567, data: [8, 2, 3]}]
var solicitUuidsValue = ['value3', 'value4'];
var serviceDataValue = [{uuid: 'uuid8', data: [1, 2, 3]},
                        {uuid: 'uuid36', data: [8, 2, 3]}]

var advertisement = {
  type: 'broadcast',
  serviceUuids: serviceUuidsValue,
  manufacturerData: manufacturerDataValue,
  solicitUuids: solicitUuidsValue,
  serviceData: serviceDataValue
};

registerAdvertisement(advertisement, function (advertisementId) {
  if (chrome.runtime.lastError || !advertisementId) {
    chrome.test.fail(chrome.runtime.lastError.message);
    return;
  }

  unregisterAdvertisement(advertisementId, function () {
    if (chrome.runtime.lastError) {
      chrome.test.fail(chrome.runtime.lastError.message);
      return;
    }
    chrome.test.succeed();
  });
});
