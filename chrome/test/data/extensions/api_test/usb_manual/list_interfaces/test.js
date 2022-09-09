// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var usb = chrome.usb;

var DEVICE_ID = {
  // Google Nexus S
  'vendorId': 6353,
  'productId': 20194
};

var tests = [
  function listInterfaces() {
    chrome.permissions.request({
        permissions: [{'usbDevices': [DEVICE_ID]}]
    }, function(granted) {
      if (!granted) {
        chrome.test.fail('Could not get optional permisson');
      } else {
        usb.findDevices(DEVICE_ID, function(devices) {
          if (typeof devices === 'undefined') {
            chrome.test.fail('Device optional_permissions seem to be missing');
          } else {
            for (var i = 0; i < devices.length; i++) {
              var device = devices[i];
              console.log('device: ' + JSON.stringify(device));
              usb.listInterfaces(device, function(result) {
                if (typeof result !== 'object') {
                  chrome.test.fail('should be object type. was: '
                      + typeof result);
                } else {
                  console.log(JSON.stringify(result));
                  chrome.test.succeed();
                }
              });
            }
          }
        });
      }
    });
  },
];

chrome.test.runTests(tests);
