// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var advertisement = {
  type: 'broadcast',
  serviceUuids: ['1234']
};

chrome.bluetoothLowEnergy.registerAdvertisement(advertisement, function() {

  if (chrome.runtime.lastError) {
    chrome.test.assertEq(chrome.runtime.lastError.message, "Permission denied");
    chrome.test.succeed();
    return;
  }
  chrome.test.fail("Should not work outside of kiosk mode");
});
