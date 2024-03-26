// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Show the page action icon for all tabs.
chrome.tabs.onCreated.addListener(function(tab) {
  chrome.windows.getCurrent(null, function(window) {
    chrome.tabs.query({windowId:window.id}, function(tabs) {
      for (var i = 0, t; t = tabs[i]; i++) {
       chrome.pageAction.show(t.id);
      }
    });
  });
});

// Turn the background red when the user clicks on the page action.
chrome.pageAction.onClicked.addListener(function(tab) {
  chrome.tabs.executeScript(
      null, {code: "document.body.style.backgroundColor='red'"});
  chrome.test.notifyPass();
});

chrome.test.notifyPass();
