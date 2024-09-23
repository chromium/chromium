// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tests whether we have permission to use individual functions, despite
// not having asked for any permissions in the manifest.

chrome.test.runTests([

  // Test the tabs API.
  function tabs() {
    try {
      chrome.tabs.create({url: "404_is_enough.html"}, function(tab1) {
        chrome.tabs.update(tab1.id, {url: "404_again.html"}, function(tab2) {
          chrome.tabs.onRemoved.addListener(function(tabId, removeInfo) {
            chrome.test.assertEq(tab1.id, tabId);
            chrome.test.succeed();
          });
          chrome.tabs.remove(tab1.id);
        });
      });
    } catch (e) {
      chrome.test.fail();
    }
  },

  // Negative test for the tabs API.
  function tabsNegative() {
    try {
      var tab = chrome.tabs.query();
      chrome.test.fail();
    } catch (e) {
      chrome.test.succeed();
    }
  }

]);
