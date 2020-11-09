// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cs = chrome.contentSettings;

function setPluginsSetting() {
  cs['plugins'].set({
    'primaryPattern': 'https://www.example.com/*',
    'secondaryPattern': '<all_urls>',
    'setting': 'allow'
  });
}

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function testPluginsApi() {
    cs['plugins'].set(
        {
          'primaryPattern': 'https://*.google.com:443/*',
          'secondaryPattern': '<all_urls>',
          'setting': 'allow'
        },
        chrome.test.callbackFail(
            '`chrome.contentSettings.plugins.set()` API is no longer supported.'));
    cs['plugins'].get(
        {'primaryUrl': 'https://drive.google.com:443/*'},
        chrome.test.callbackFail(
            '`chrome.contentSettings.plugins.get()` API is no longer supported.'));
    cs['plugins'].clear(
        {},
        chrome.test.callbackFail(
            '`chrome.contentSettings.plugins.clear()` API is no longer supported.'));
  },
]);