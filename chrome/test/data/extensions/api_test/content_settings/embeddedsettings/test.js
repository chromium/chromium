// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var cs = chrome.contentSettings;
var delegation;

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      delegation = config.customArg;
      chrome.test.succeed();
    });
  },
  function embeddedSettings() {
    // Cookies is not impacted by permission delegation and embedded patterns
    // are permitted even when it's enabled.
    cs['cookies'].set({
        primaryPattern: 'http://google.com/*',
        secondaryPattern: 'http://example.com/*',
        setting: 'allow'
      }, chrome.test.callbackPass());

    // Geolocation embedded patterns are not permitted when permission
    // delegation is enabled.
    if (delegation  == 'permission') {
      cs['location'].set({
        primaryPattern: 'http://google.com/*',
        secondaryPattern: 'http://example.com/*',
        setting: 'allow'
      }, chrome.test.callbackFail(
          'Embedded patterns are not supported for this setting.'));
    } else {
      cs['location'].set({
        primaryPattern: 'http://google.com/*',
        secondaryPattern: 'http://example.com/*',
        setting: 'allow'
      }, chrome.test.callbackPass());
    }
  }
]);
