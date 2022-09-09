// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testDeviceInfo() {
  chrome.test.assertEq(2, devices.length);

  chrome.test.assertEq('Chromebook Pixel', devices[0].name);
  chrome.test.assertEq(0x080104, devices[0].deviceClass);
  chrome.test.assertEq('bluetooth', devices[0].vendorIdSource);
  chrome.test.assertEq(0x00E0, devices[0].vendorId);
  chrome.test.assertEq(0x240A, devices[0].productId);
  chrome.test.assertEq(0x0400, devices[0].deviceId);
  chrome.test.assertEq('computer', devices[0].type);

  chrome.test.assertEq(2, devices[0].uuids.length);

  let uuids = new Set(devices[0].uuids);
  chrome.test.assertTrue(uuids.has('00001105-0000-1000-8000-00805f9b34fb'));
  chrome.test.assertTrue(uuids.has('00001106-0000-1000-8000-00805f9b34fb'));

  chrome.test.assertEq('d2', devices[1].name);
  chrome.test.assertEq(0, devices[1].deviceClass);
  chrome.test.assertEq(undefined, devices[1].vendorIdSource);
  chrome.test.assertEq(undefined, devices[1].vendorId);
  chrome.test.assertEq(undefined, devices[1].productId);
  chrome.test.assertEq(undefined, devices[1].deviceId);
  chrome.test.assertEq(undefined, devices[1].type);
  chrome.test.assertEq(0, devices[1].uuids.length);

  chrome.test.succeed();
}

var devices = [];

function failOnError() {
  if (chrome.runtime.lastError) {
    chrome.test.fail(chrome.runtime.lastError.message);
  }
}

chrome.bluetooth.getDevices(
  function(result) {
    failOnError();
    devices = result;
    chrome.test.sendMessage('ready',
      function(message) {
        chrome.test.runTests([testDeviceInfo]);
      });
  });
