// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.test.getConfig(function(config) {
    var testName = config.customArg;
    if (!testName) {
      chrome.test.fail("Missing test name.");
      return;
    }
    chrome.chromeosInfoPrivate.get([
      'sessionType',
      'playStoreStatus',
      'managedDeviceStatus',
      'deviceType',
      'stylusStatus',
      'assistantStatus',
    ], chrome.test.callbackPass(function(values) {
          switch (testName) {
            case 'kiosk':
              chrome.test.assertEq('kiosk', values['sessionType']);
              break;
            case 'arc not-available':
              chrome.test.assertEq('not available', values['playStoreStatus']);
              break;
            case 'arc available':
              chrome.test.assertEq('available', values['playStoreStatus']);
              break;
            case 'arc enabled':
              chrome.test.assertEq('enabled', values['playStoreStatus']);
              break;
            case 'managed':
              chrome.test.assertEq('managed', values['managedDeviceStatus']);
              break;
            case 'chromebase':
              chrome.test.assertEq('chromebase', values['deviceType']);
              break;
            case 'chromebit':
              chrome.test.assertEq('chromebit', values['deviceType']);
              break;
            case 'chromebook':
              chrome.test.assertEq('chromebook', values['deviceType']);
              break;
            case 'chromebox':
              chrome.test.assertEq('chromebox', values['deviceType']);
              break;
            case 'unknown device type':
              chrome.test.assertEq('chromedevice', values['deviceType']);
              break;
            case 'stylus unsupported':
              chrome.test.assertEq('unsupported', values['stylusStatus']);
              break;
            case 'stylus supported':
              chrome.test.assertEq('supported', values['stylusStatus']);
              break;
            case 'stylus seen':
              chrome.test.assertEq('seen', values['stylusStatus']);
              break;
            case 'assistant supported':
              chrome.test.assertEq('supported', values['assistantStatus']);
              break;
          }
        }));
  });
});
