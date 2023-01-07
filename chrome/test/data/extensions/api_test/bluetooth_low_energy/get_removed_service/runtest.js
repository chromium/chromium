// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.bluetoothLowEnergy.onServiceAdded.addListener(function (result) {
  // getService should return this service.
  chrome.bluetoothLowEnergy.getService(result.instanceId, function (service) {
    if (chrome.runtime.lastError) {
      chrome.test.sendMessage(chrome.runtime.lastError.message);
    }

    chrome.test.assertEq(result.instanceId, service.instanceId);

    chrome.test.sendMessage('getServiceSuccess');
  });
});

chrome.bluetoothLowEnergy.onServiceRemoved.addListener(function (result) {
  // getService should return error.
  chrome.bluetoothLowEnergy.getService(result.instanceId, function (service) {
    if (service || !chrome.runtime.lastError) {
      chrome.test.sendMessage('Call to getService should have failed.');
    }

    chrome.test.sendMessage('getServiceFail', function (message) {
      chrome.test.succeed();
    });
  });
});
