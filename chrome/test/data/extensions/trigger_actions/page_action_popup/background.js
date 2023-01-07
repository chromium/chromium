// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Show the page action icon for all tabs.
chrome.tabs.getAllInWindow(null, function(tabs) {
  for (var i = 0, t; t = tabs[i]; i++)
    chrome.pageAction.show(t.id);
  chrome.test.sendMessage('ready');
});
