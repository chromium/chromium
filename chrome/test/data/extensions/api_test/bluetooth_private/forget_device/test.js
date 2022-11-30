// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var deviceAddress = '11:12:13:14:15:16';

function testForgetDevice() {
  chrome.bluetooth.getDevice(deviceAddress, function(device) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(deviceAddress, device.address);
    chrome.bluetoothPrivate.forgetDevice(deviceAddress, function() {
      chrome.test.assertNoLastError();
      chrome.bluetooth.getDevice(deviceAddress, function(device) {
        chrome.test.assertLastError('Invalid device');
        chrome.test.succeed();
      });
    });
  });
}

chrome.test.runTests([testForgetDevice]);
