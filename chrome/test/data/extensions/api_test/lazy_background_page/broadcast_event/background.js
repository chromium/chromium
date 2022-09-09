// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (tab.url.search("test_file.html") > -1)
    chrome.pageAction.show(tabId);
  else
    chrome.pageAction.hide(tabId);
});
