// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress = '11:12:13:14:15:16';

function testConnect() {
  chrome.bluetoothPrivate.connect(deviceAddress, function(result1) {
    chrome.test.assertEq('success', result1);
    chrome.bluetoothPrivate.connect(deviceAddress, function(result2) {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('alreadyConnected', result2);
      chrome.test.succeed();
    });
  });
}

chrome.test.runTests([testConnect]);
