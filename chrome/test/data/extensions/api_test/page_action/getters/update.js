// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;

chrome.tabs.query({active: true}, function(tabs) {
  const tab = tabs[0];
  chrome.test.runTests([
    function getPopup() {
      chrome.pageAction.getPopup({tabId: tab.id}, pass(function(result) {
        chrome.test.assertTrue(
            /chrome-extension\:\/\/[a-p]{32}\/Popup\.html/.test(result));
      }));
    },

    function getTitle() {
      chrome.pageAction.getTitle({tabId: tab.id}, pass(function(result) {
        chrome.test.assertEq("Title", result);
      }));
    }
  ]);
});
