// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress = '11:12:13:14:15:16';
var errorPairingNotEnabled = 'Pairing not enabled';

function testPair() {
  chrome.bluetoothPrivate.pair(deviceAddress, function() {
    chrome.test.assertEq(
        errorPairingNotEnabled, chrome.runtime.lastError.message);

    // onPairing listener must be provided for pair to succeed.
    chrome.bluetoothPrivate.onPairing.addListener(function(pairingEvent) {
      chrome.test.assertEq('confirmPasskey', pairingEvent.pairing);
      chrome.bluetoothPrivate.setPairingResponse({
        device: pairingEvent.device,
        response: 'confirm',
      }, function() {
        chrome.test.assertNoLastError();
      });
    });

    chrome.bluetoothPrivate.pair(deviceAddress, function() {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  });
}

chrome.test.runTests([testPair]);
