// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.query({active: true}, function(tabs) {
  // When the browser action is clicked, add a popup.
  chrome.browserAction.onClicked.addListener(function(tab) {
    chrome.browserAction.setPopup({
      tabId: tab.id,
      popup: 'a_popup.html'
    });
    chrome.test.notifyPass();
  });
  chrome.test.notifyPass();
});
