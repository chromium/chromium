// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabId = -1;

chrome.pageAction.onClicked.addListener(function(tab) {
  chrome.pageAction.hide(tabId);
  chrome.test.sendMessage('clicked');
});

chrome.tabs.query({active: true}, function(tabs) {
  tabId = tabs[0].id;
  // Callbacks should be not be required:
  chrome.pageAction.hide(tabId);
  chrome.pageAction.show(tabId);

  // Callbacks should be permitted:
  chrome.pageAction.show(tabId, function() {
    chrome.test.assertNoLastError();

    chrome.pageAction.show(123456789, function() {
      chrome.test.assertLastError('No tab with id: 123456789.');

      chrome.pageAction.hide(987654321, function() {
        chrome.test.assertLastError('No tab with id: 987654321.');

        chrome.test.sendMessage('ready');
      });
    });
  });
});
