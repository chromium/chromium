// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getRegistrationCodeTest() {
    var expected_code = '';
    // TODO(gauravsh): Mock out StatisticsProvider to make getCouponCode()
    // return a well known value for brower_tests.
    chrome.echoPrivate.getRegistrationCode('COUPON_CODE',
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == expected_code);
    }));
    chrome.echoPrivate.getRegistrationCode('GROUP_CODE',
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == expected_code);
    }));
    chrome.echoPrivate.getRegistrationCode('INVALID_CODE',
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == '');
    }));
    chrome.echoPrivate.getOobeTimestamp(
        chrome.test.callbackPass(function(result) {
      chrome.test.assertTrue(result == '');
    }));
  }
]);
