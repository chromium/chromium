// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that we can change various properties of the browser action.
// The C++ verifies.
chrome.tabs.query({active: true}, function(tabs) {
  const tab = tabs[0];
  chrome.pageAction.show(tab.id);
  chrome.pageAction.setTitle({title: "Modified", tabId: tab.id});

  chrome.test.notifyPass();
});
