// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const usb = chrome.usb;

const genericTransfer = {
  'direction': 'in',
  'endpoint': 1,
  'length': 0,
  'timeout': -1,
};
const controlTransfer = {
  'index': 0,
  'direction': 'in',
  'requestType': 'standard',
  'recipient': 'device',
  'request': 0,
  'value': 0,
  'length': 0,
  'timeout': -1,
};
const isoTransfer = {
  'packetLength': 0,
  'transferInfo': genericTransfer,
  'packets': 0,
};
const errorTimeout = 'Transfer timeout must be greater than or equal to 0.';

function createInvalidTransferTest(usbFunction, transferInfo) {
  return function() {
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      const device = devices[0];
      usbFunction(device, transferInfo, chrome.test.callbackFail(errorTimeout));
    });
  };
}

const tests = [
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer),
];

chrome.test.runTests(tests);
