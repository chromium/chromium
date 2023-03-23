// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test.
// Run with browser_tests:
//     --gtest_filter=ExtensionContentSettingsApiTest.Incognito*
//
// Arguments: [Permission]
// Example Arguments: "allow"

'use strict';

var cs = chrome.contentSettings;

var givenPermission;

var settings = [
  'cookies',
  'images',
  'javascript',
  'popups',
  'location',
  'notifications',
  'microphone',
  'camera',
  'automaticDownloads'
];

// Settings that do not support site-specific exceptions.
var globalOnlySettings = ['autoVerify'];

function expect(expected, message) {
  return chrome.test.callbackPass(function(value) {
    chrome.test.assertEq(expected, value, message);
  });
}

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      givenPermission = config.customArg;
      chrome.test.succeed();
    });
  },
  function setContentSettings() {
    settings.forEach(function(type) {
      cs[type].set({
        'primaryPattern': 'http://*.example.com/*',
        'secondaryPattern': 'http://*.example.com/*',
        'setting': givenPermission,
        'scope': 'incognito_session_only'
      }, chrome.test.callbackPass());
    });
  },
  function setGlobalContentSettings() {
    globalOnlySettings.forEach(function(type) {
      cs[type].set(
          {
            'primaryPattern': '<all_urls>',
            'secondaryPattern': '<all_urls>',
            'setting': givenPermission,
            'scope': 'incognito_session_only'
          },
          chrome.test.callbackPass());
    });
  },
  function getContentSettings() {
    [...settings, ...globalOnlySettings].forEach(function(type) {
      var message = 'Setting for ' + type + ' should be ' + givenPermission;
      cs[type].get({
        'primaryUrl': 'http://www.example.com',
        'secondaryUrl': 'http://www.example.com'
      }, expect({'setting':givenPermission}, message));
    });
  },
]);
