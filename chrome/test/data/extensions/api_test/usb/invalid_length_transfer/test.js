// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const usb = chrome.usb;

const genericTransfer = {
  'direction': 'in',
  'endpoint': 1,
  'length': -1,
};
const controlTransfer = {
  'index': 0,
  'direction': 'in',
  'requestType': 'standard',
  'recipient': 'device',
  'request': 0,
  'value': 0,
  'length': -1,
};
const isoTransfer = {
  'packetLength': 0,
  'transferInfo': genericTransfer,
  'packets': 0,
};
const errorTransferLength = 'Transfer length must be a ' +
    'positive number less than 104,857,600.';
const errorNumPackets = 'Number of packets must be a ' +
    'positive number less than 4,194,304.';
const errorPacketLength = 'Packet length must be a ' +
    'positive number less than 65,536.';
const errorInsufficientTransferLength = 'Transfer length is insufficient.';
const largeSize = 100 * 1024 * 1024 + 1;
const maxPackets = 4 * 1024 * 1024;
const maxPacketLength = 64 * 1024;

function createInvalidTransferTest(usbFunction, transferInfo, transferLength) {
  return function() {
    genericTransfer['length'] = transferLength;
    controlTransfer['length'] = transferLength;
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      const device = devices[0];
      usbFunction(
          device, transferInfo,
          chrome.test.callbackFail(errorTransferLength, function() {}));
    });
  };
}

function createInvalidPacketLengthTest(
    transferLength, packets, packetLength, errorMessage) {
  return function() {
    genericTransfer['length'] = transferLength;
    isoTransfer['packets'] = packets;
    isoTransfer['packetLength'] = packetLength;
    usb.findDevices({vendorId: 0, productId: 0}, function(devices) {
      const device = devices[0];
      usb.isochronousTransfer(
          device, isoTransfer,
          chrome.test.callbackFail(errorMessage, function() {}));
    });
  };
}

const tests = [
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer, -1),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer, -1),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer, -1),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer, -1),
  createInvalidTransferTest(usb.bulkTransfer, genericTransfer, largeSize),
  createInvalidTransferTest(usb.controlTransfer, controlTransfer, largeSize),
  createInvalidTransferTest(usb.interruptTransfer, genericTransfer, largeSize),
  createInvalidTransferTest(usb.isochronousTransfer, isoTransfer, largeSize),

  createInvalidPacketLengthTest(1024, -1, 0, errorNumPackets),
  createInvalidPacketLengthTest(maxPackets, maxPackets, 0, errorNumPackets),
  createInvalidPacketLengthTest(1024, 100, -1, errorPacketLength),
  createInvalidPacketLengthTest(1024, 100, maxPacketLength, errorPacketLength),
  createInvalidPacketLengthTest(1024, 1025, 0, errorInsufficientTransferLength),
];

chrome.test.runTests(tests);
