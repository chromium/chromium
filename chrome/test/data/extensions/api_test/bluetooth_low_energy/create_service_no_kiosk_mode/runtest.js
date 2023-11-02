// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var service = { uuid: '00001234-0000-1000-8000-00805f9b34fb', isPrimary: true }
chrome.bluetoothLowEnergy.createService(service, function(result) {
  if (chrome.runtime.lastError) {
    chrome.test.assertEq(chrome.runtime.lastError.message, "Permission denied");
    chrome.test.succeed();
    return;
  }
  chrome.test.fail("Should not work outside of kiosk mode");
});
