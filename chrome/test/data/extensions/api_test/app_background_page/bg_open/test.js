// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This test has two parts - an extension part and a hosted app part.
// This is the extension background page, which waits for the background
// contents to open a popup window.

// See if the background contents loaded first and already opened the tabs.
chrome.tabs.query({}, function(tabs) {
  for (var i = 0; i < tabs.length; ++i) {
    if (tabs[i].url.match("popup\.html$")) {
      chrome.test.notifyPass();
      return;
    }
  }
  // No tab loaded yet - add a listener and wait for it to load.
  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (tab.url.match("popup\.html$")) {
      chrome.test.notifyPass();
    }
  });
});
