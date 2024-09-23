// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.query({active: true}, function(tabs) {
  chrome.pageAction.setPopup({
    tabId: tabs[0].id,
    popup: ''  // No popup.
  });
  chrome.test.notifyPass();
});
