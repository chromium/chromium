// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var $ = document.getElementById.bind(document);

function checkOneDevice() {
  var devices = $('register-device-list').children;
  assertEquals(1, devices.length);
  var firstDevice = devices[0];

  assertDomElementIsSamplePrinter(firstDevice);
}

function checkNoDevices() {
  assertEquals(0, $('register-device-list').children.length);
}

function registerShowOverlay() {
  var button = document.querySelector('#register-device-list button');
  var overlay = $('overlay');

  assertTrue(button != null);

  assertTrue(overlay.hidden);
  button.click();
  assertFalse(overlay.hidden);

  assertFalse($('register-page-confirm').hidden);
}

function registerBegin() {
  var button = $('register-continue');
  assertTrue(button != null);

  assertFalse($('register-page-confirm').hidden);
  button.click();
  assertTrue($('register-page-confirm').hidden);
  assertFalse($('register-printer-page-adding1').hidden);
}

function expectPageAdding1() {
  assertFalse($('register-printer-page-adding1').hidden);
}

function expectPageAdding2() {
  assertFalse($('register-page-adding2').hidden);
}

function expectRegisterDone() {
  assertTrue($('overlay').hidden);
  var cloudDevices = $('cloud-devices');
  var firstDevice = cloudDevices.firstChild;
  assertDomElementIsSamplePrinter(firstDevice);
}

function assertDomElementIsSamplePrinter(device) {
  var deviceName = device.querySelector('.device-name').textContent;
  assertEquals('Sample device', deviceName);

  var deviceDescription = device.querySelector('.device-subline').textContent;
  assertEquals('Sample device description', deviceDescription);

  var button = device.querySelector('button');
  // Button should not be disabled since there is a logged in user.
  assertFalse(button.disabled);
}
