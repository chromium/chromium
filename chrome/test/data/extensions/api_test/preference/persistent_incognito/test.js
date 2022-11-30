// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.PreferencePersistentIncognito

var pw = chrome.privacy.websites;
function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(expected, value, message);
  });
}
chrome.test.runTests([
  function getRegular() {
    pw.thirdPartyCookiesAllowed.get(
        {},
        expect({ 'value': true,
                 'levelOfControl': "controllable_by_this_extension" },
               "third-party cookies should not be blocked"));
  },
  function getIncognito() {
    pw.thirdPartyCookiesAllowed.get(
        { 'incognito': true },
        expect({ 'value': true,
                 'incognitoSpecific': false,
                 'levelOfControl': "controllable_by_this_extension" },
               "third-party cookies should not be blocked in incognito mode"));
  },
  function set() {
    pw.thirdPartyCookiesAllowed.set(
        { 'scope': 'incognito_persistent', 'value': false },
        chrome.test.callbackPass());
  },
  function getRegular2() {
    pw.thirdPartyCookiesAllowed.get(
        {},
        expect({ 'value': true,
                 'levelOfControl': "controllable_by_this_extension" },
               "third-party cookies should not be blocked"));
  },
  function getIncognito2() {
    pw.thirdPartyCookiesAllowed.get(
        { 'incognito': true },
        expect({ 'value': false,
                 'incognitoSpecific': true,
                 'levelOfControl': "controlled_by_this_extension" },
               "third-party cookies should be blocked in incognito mode"));
  },
]);
