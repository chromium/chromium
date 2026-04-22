// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testGenerateAppForLink(url, title, error, func) {
  chrome.test.runWithUserGesture(function() {
    if (error) {
      chrome.management.generateAppForLink(
          url, title, callback(function(data) {}, error));
    } else {
      chrome.management.generateAppForLink(url, title, callback(func));
    }
  });
}

const tests = [
  function generateAppForLinkWithoutUserGesture() {
    chrome.management.generateAppForLink(
        'http://google.com', 'test', callback(function() {
        }, 'chrome.management.generateAppForLink requires a user gesture.'));
  },

  function generateAppForInvalidLink() {
    testGenerateAppForLink('', 'test', 'The URL "" is invalid.');
    testGenerateAppForLink('aaaa', 'test', 'The URL "aaaa" is invalid.');
    testGenerateAppForLink(
        'http1://google.com', 'test',
        'The URL "http1://google.com" is invalid.');
    testGenerateAppForLink(
        'chrome://about', 'test', 'The URL "chrome://about" is invalid.');
    testGenerateAppForLink(
        'chrome-extension://test/test', 'test',
        'The URL "chrome-extension://test/test" is invalid.');
  },

  function generateAppWithEmptyTitle() {
    testGenerateAppForLink(
        'http://google.com', '', 'The title can not be empty.');
  },

  function generateAppForLinkWithShortURL() {
    const url = 'http://google.com';
    const title = 'testApp';
    testGenerateAppForLink(url, title, null, function(data) {
      assertEq('http://google.com/', data.appLaunchUrl);
      assertEq(title, data.name);
      // There is no manifest for the test page so no icons are specified.
      assertEq(0, data.icons.length);
    });
  },

  function generateAppForLinkWithLongURL() {
    const url = 'http://google.com/page/page?aa=bb&cc=dd';
    const title = 'test App 2';
    testGenerateAppForLink(url, title, null, function(data) {
      assertEq(url, data.appLaunchUrl);
      assertEq(title, data.name);
      // There is no manifest for the test page so no icons are specified.
      assertEq(0, data.icons.length);
    });
  },
];

const SCRIPT_URL = '_test_resources/api_test/management/common.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.runTests(tests);
});
