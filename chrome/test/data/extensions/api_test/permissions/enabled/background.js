// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// All of the calls to chrome.* functions should succeed, since this extension
// has requested all required permissions.

var pass = chrome.test.callbackPass;

chrome.test.runTests([
  function history() {
    try {
      var query = { 'text': '', 'maxResults': 1 };
      chrome.history.search(query, pass(function(results) {}));
    } catch (e) {
      chrome.test.fail();
    }
  },

  function bookmarks() {
    try {
      chrome.bookmarks.get("1", pass(function(results) {}));
    } catch (e) {
      chrome.test.fail();
    }
  },

  function tabs() {
    try {
      chrome.tabs.query({active: true}, pass(function(results) {}));
    } catch (e) {
      chrome.test.fail();
    }
  }
]);
