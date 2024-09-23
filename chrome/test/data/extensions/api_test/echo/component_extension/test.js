// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getRegistrationCodeTest() {
    // Expected values from
    // /chrome/browser/chromeos/extensions/echo_private/echo_private_apitest.cc
    chrome.echoPrivate.getRegistrationCode('COUPON_CODE',
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(result == "COUPON_CODE");
    }));
    chrome.echoPrivate.getRegistrationCode('GROUP_CODE',
        chrome.test.callbackPass(function(result) {
          chrome.test.assertTrue(result == "GROUP_CODE");
    }));
    chrome.echoPrivate.getRegistrationCode('INVALID_CODE',
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == '');
    }));
    chrome.echoPrivate.getOobeTimestamp(
        chrome.test.callbackPass(function(result) {
          // Date of the 13th week in 2024 (VPD mock provides "2024-13")
          chrome.test.assertTrue(result == '2024-3-25');
    }));
  }
]);
