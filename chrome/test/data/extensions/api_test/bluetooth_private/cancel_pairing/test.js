// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testCancelPairing() {
  chrome.bluetoothPrivate.onPairing.addListener(function(pairingEvent) {
    chrome.test.assertEq('requestAuthorization', pairingEvent.pairing);
    chrome.bluetoothPrivate.setPairingResponse({
        device: pairingEvent.device,
        response: 'cancel',
    }, function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  });
}

chrome.test.runTests([testCancelPairing]);
