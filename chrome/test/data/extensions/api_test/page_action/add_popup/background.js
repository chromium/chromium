// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.getSelected(null, function(tab) {
  chrome.pageAction.show(tab.id);

  // When the page action icon is clicked for the first time, add a popup.
  chrome.pageAction.onClicked.addListener(function(tab) {
    chrome.pageAction.setPopup({
      tabId: tab.id,
      popup: 'a_popup.html'
    });
    chrome.test.notifyPass();
  });

  chrome.test.notifyPass();
});
