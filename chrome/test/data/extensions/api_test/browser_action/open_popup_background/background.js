// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status === 'complete') {
    chrome.browserAction.openPopup(function(popup) {
      chrome.test.sendMessage(
          popup ? 'opened' :
              'failed: ' + chrome.runtime.lastError ?
                  chrome.runtime.lastError.message :
                  'unknown error');
    });
  }
});
