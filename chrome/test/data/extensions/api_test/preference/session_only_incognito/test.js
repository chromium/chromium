// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.PreferenceSessionOnlyIncognito

var pw = chrome.privacy.websites;
function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertNoLastError();
    chrome.test.assertEq(expected, value, message);
  });
}
chrome.test.runTests([
  function getRegular() {
    pw.hyperlinkAuditingEnabled.get(
        {},
        expect({ 'value': true,
                 'levelOfControl': "controllable_by_this_extension" },
               "hyperlink auditing should be enabled"));
  },
  function getIncognito() {
    pw.hyperlinkAuditingEnabled.get(
        { 'incognito': true },
        expect({ 'value': true,
                 'incognitoSpecific': false,
                 'levelOfControl': "controllable_by_this_extension" },
               "hyperlink auditing should be enabled in incognito mode"));
  },
  function set() {
    pw.hyperlinkAuditingEnabled.set(
        { 'scope': 'incognito_persistent', 'value': false },
        chrome.test.callbackPass());
  },
  function getRegular2() {
    pw.hyperlinkAuditingEnabled.get(
        {},
        expect({ 'value': true,
                 'levelOfControl': "controllable_by_this_extension" },
               "hyperlink auditing should be enabled"));
  },
  function getIncognito2() {
    pw.hyperlinkAuditingEnabled.get(
        { 'incognito': true },
        expect({ 'value': false,
                 'incognitoSpecific': true,
                 'levelOfControl': "controlled_by_this_extension" },
               "hyperlink auditing should be disabled in incognito mode"));
  },
  // We cannot set session_only_persistent preferences if there is no incognito
  // session.
  function set2() {
    pw.hyperlinkAuditingEnabled.set(
        { 'scope': 'incognito_session_only', 'value': true },
        chrome.test.callbackFail("You cannot set a preference with scope " +
                                 "'incognito_session_only' when no incognito " +
                                 "window is open."));
  },
  function openIncognito() {
    chrome.windows.create({incognito: true}, chrome.test.callbackPass());
  },
  // session_only_persistent overrides incognito_persistent.
  function set3() {
    pw.hyperlinkAuditingEnabled.set(
        { 'scope': 'incognito_session_only', 'value': true },
        chrome.test.callbackPass());
  },
  function getRegular3() {
    pw.hyperlinkAuditingEnabled.get(
        {},
        expect({ 'value': true,
                 'levelOfControl': "controllable_by_this_extension" },
               "hyperlink auditing should be enabled"));
  },
  function getIncognito3() {
    pw.hyperlinkAuditingEnabled.get(
        { 'incognito': true },
        expect({ 'value': true,
                 'incognitoSpecific': true,
                 'levelOfControl': "controlled_by_this_extension" },
               "hyperlink auditing should be enabled in incognito mode"));
  },
]);
