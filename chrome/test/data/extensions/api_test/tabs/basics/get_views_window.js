// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function canGetViewsOfEmptyWindow() {
    testGetNewWindowView({type: "normal"}, []);
  },

  function canGetViewsOfWindowWithUrl() {
    var URLS = ["a.html"];
    testGetNewWindowView({type: "normal", url: URLS}, URLS);
  },

  function canGetViewsOfWindowWithManyUrls() {
    var URLS = ["a.html", "b.html", "c.html"];
    testGetNewWindowView({type: "normal", url: URLS}, URLS);
  },

  function canGetViewsOfDetatchedTab() {
    // Start with a window holding two tabs.
    chrome.windows.create({"url": ["a.html", "b.html"]}, function(win) {
      // Detatch the first tab.  See that the tab is findable with the new
      // window ID.
      testGetNewWindowView(
          {type: "normal", tabId: win.tabs[0].id}, ["a.html"]);
    });
  }
]);
