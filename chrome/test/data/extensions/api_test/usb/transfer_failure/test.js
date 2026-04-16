// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

function createErrorTest(resultCode, errorMessage) {
  return function() {
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      var device = devices[0];
      var transfer = new Object();
      transfer.direction = "out";
      transfer.endpoint = 1;
      transfer.data = new ArrayBuffer(0);
      usb.bulkTransfer(device, transfer, function (result) {
        if (errorMessage) {
          chrome.test.assertLastError(errorMessage);
        } else {
          chrome.test.assertNoLastError();
        }
        chrome.test.assertTrue(resultCode == result.resultCode);
        chrome.test.succeed();
      });
    });
  };
}

function createIsochronousErrorTest(resultCode, errorMessage) {
  return function() {
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      var device = devices[0];
      var transfer = {
        'transferInfo': {
          'direction': "in",
          'endpoint': 2,
          'length': 160
        },
        'packets': 10,
        'packetLength': 16
      };
      usb.isochronousTransfer(device, transfer, function (result) {
        if (errorMessage) {
          chrome.test.assertLastError(errorMessage);
          // Device responds with only 8-byte packets and the second half fail.
          chrome.test.assertTrue(result.data.byteLength == 40);
        } else {
          chrome.test.assertNoLastError();
          // Device responds with a full set of 10 8-byte packets.
          chrome.test.assertTrue(result.data.byteLength == 80);
        }
        chrome.test.assertTrue(resultCode == result.resultCode);
        chrome.test.succeed();
      });
    });
  };
}

var tests = [
  createErrorTest(0, undefined),
  createErrorTest(1, "Transfer failed."),
  createErrorTest(2, "Transfer timed out."),
  createIsochronousErrorTest(0, undefined),
  createIsochronousErrorTest(1, "Transfer failed."),
];

chrome.test.runTests(tests);
