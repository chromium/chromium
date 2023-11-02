// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

chrome.test.runTests([
  function getBadgeText() {
    chrome.browserAction.getBadgeText({}, pass(function(result) {
      chrome.test.assertEq("Text", result);
    }));
  },

  function getBadgeBackgroundColor() {
    chrome.browserAction.getBadgeBackgroundColor({}, pass(function(result) {
      chrome.test.assertEq([255, 0, 0, 255], result);
    }));
  },

  function getPopup() {
    chrome.browserAction.getPopup({}, pass(function(result) {
      chrome.test.assertTrue(
          /chrome-extension\:\/\/[a-p]{32}\/Popup\.html/.test(result));
    }));
  },

  function getTitle() {
    chrome.browserAction.getTitle({}, pass(function(result) {
      chrome.test.assertEq("Title", result);
    }));
  }
]);
