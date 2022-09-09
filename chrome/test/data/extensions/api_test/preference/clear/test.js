// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.PreferenceClear

var pw = chrome.privacy.websites;
chrome.test.runTests([
  function getThirdPartyCookiesAllowed() {
    pw.thirdPartyCookiesAllowed.get({}, chrome.test.callbackPass(
        function(allowed) {
          chrome.test.assertEq(
              allowed,
              {
                'value': false,
                'levelOfControl': "controllable_by_this_extension"
              },
              "third-party cookies should be blocked");
        }));
  },
  function setThirdPartyCookiesAllowed() {
    pw.thirdPartyCookiesAllowed.set(
        {'value': true},
        chrome.test.callbackPass());
  },
  function clearThirdPartyCookiesAllowed() {
    pw.thirdPartyCookiesAllowed.clear({}, chrome.test.callbackPass());
  },
  function getThirdPartyCookiesAllowed2() {
    pw.thirdPartyCookiesAllowed.get({}, chrome.test.callbackPass(
        function(allowed) {
          chrome.test.assertEq(
              allowed,
              {
                'value': false,
                'levelOfControl': "controllable_by_this_extension"
              },
              "third-party cookies should be blocked");
        }));
  }
]);
