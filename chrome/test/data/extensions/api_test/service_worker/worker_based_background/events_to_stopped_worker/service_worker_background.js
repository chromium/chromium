// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var seenLoading = false;

chrome.tabs.onUpdated.addListener(function localListener (
    tabId, changeInfo, tab) {
  if (changeInfo.status === 'loading') {
    if (seenLoading == true) {
      chrome.test.sendMessage('ERROR');
    } else {
      seenLoading = true;
    }
  } else if (changeInfo.status === 'complete') {
    chrome.test.sendMessage(seenLoading == true ? 'finished' : 'ERROR');
  }
});

chrome.test.sendMessage('ready');
