// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All of the calls to chrome.* functions should fail, with the exception of
// chrome.tabs.*, since this extension has requested no permissions.

chrome.test.runTests([
  function history() {
    try {
      var query = { 'text': '', 'maxResults': 1 };
      chrome.history.search(query, function(results) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  },

  function bookmarks() {
    try {
      // Use getRecent() instead of get("1") because desktop Android doesn't
      // create the bookmark bar (id "1") by default.
      chrome.bookmarks.getRecent(1, function (results) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  },

  // Tabs functionality should be enabled even if the tabs permissions are not
  // present.
  function tabs() {
    // TODO(crbug.com/371432155): Port to desktop Android when chrome.tabs API
    // is available.
    const getPlatformInfo = new Promise((resolve) => {
      chrome.runtime.getPlatformInfo(info => resolve(info.os == 'android'));
    });
    getPlatformInfo.then(isAndroid => {
      if (isAndroid) {
        chrome.test.succeed();
        return;
      } else {
        try {
          chrome.tabs.create({'url': '1'}, function(tab) {
            // Tabs strip sensitive data without permissions.
            chrome.test.assertFalse('url' in tab);
            chrome.test.succeed();
          });
        } catch (e) {
          chrome.test.fail();
        }
      }
    });
  },

  function idle() {
    try {
      chrome.idle.queryState(60, function(state) {
        chrome.test.fail();
      });
    } catch (e) {
      chrome.test.succeed();
    }
  }
]);
