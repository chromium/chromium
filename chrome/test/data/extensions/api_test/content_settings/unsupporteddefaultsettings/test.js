// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests --gtest_filter=ExtensionApiTest.ContentSettings

var cs = chrome.contentSettings;

// The following type-value pairs indicate that the value should be supported
// by its respective type for exceptions, but not as the default setting.
var settings = {
  "camera": "allow",
  "microphone": "allow"
};

Object.prototype.forEach = function(f) {
  var k;
  for (k in this) {
    if (this.hasOwnProperty(k))
      f(k, this[k]);
  }
};

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

function expectFalse(message) {
  return expect({
    "value": false,
    "levelOfControl": "controllable_by_this_extension"
  }, message);
}

chrome.test.runTests([
  function setDefaultContentSettings() {
    settings.forEach(function(type, setting) {
      cs[type].set({
        'primaryPattern': '<all_urls>',
        'secondaryPattern': '<all_urls>',
        'setting': setting
      },
      chrome.test.callbackFail("'" + setting +
          "' is not supported as the default setting of " + type + "."));
    });
  },
  function setExceptions() {
    settings.forEach(function(type, setting) {
      cs[type].set({
        'primaryPattern': 'http://*.google.com/*',
        'secondaryPattern': 'http://*.google.com/*',
        'setting': setting
      }, chrome.test.callbackPass());
    });
  }
]);
