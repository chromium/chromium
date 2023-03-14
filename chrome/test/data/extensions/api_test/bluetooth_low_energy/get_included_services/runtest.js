// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var error;

function testGetIncludedServices() {
  if (error !== undefined) {
    chrome.test.sendMessage('fail');
    chrome.test.fail(error);
  }
  chrome.test.assertNe(null, services, '\'services\' is null');
  chrome.test.assertEq(1, services.length);
  chrome.test.assertEq(includedId, services[0].instanceId);

  chrome.test.succeed();
}

var serviceId = 'service_id0';
var includedId = 'service_id1';
var services = null;

function earlyError(message) {
  error = message;
  chrome.test.runTests([testGetIncludedServices]);
}

function failOnError() {
  if (chrome.runtime.lastError) {
    earlyError(chrome.runtime.lastError.message);
    return true;
  }
  return false;
}

chrome.bluetoothLowEnergy.getIncludedServices(serviceId, function (result) {
  // No mapping for |serviceId|.
  if (result || !chrome.runtime.lastError) {
    earlyError('getIncludedServices should have failed');
    return;
  }

  chrome.test.sendMessage('ready', function (message) {
    chrome.bluetoothLowEnergy.getIncludedServices(serviceId, function (result) {
      if (failOnError())
        return;

      if (!result || result.length != 0) {
        earlyError('Included services should be empty.');
        return;
      }

      chrome.bluetoothLowEnergy.getIncludedServices(serviceId,
          function (result) {
            if (failOnError())
              return;

            services = result;

            chrome.test.sendMessage('ready', function (message) {
              chrome.test.runTests([testGetIncludedServices]);
            });
          });
    });
  });
});
