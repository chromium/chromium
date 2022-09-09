// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onMoved.addListener(function localListener(tabId, moveInfo) {
  chrome.tabs.onMoved.removeListener(localListener);
  chrome.test.sendMessage('moved-tab');
});

// Tell the C++ side of the test that the onCreated listener was added.
chrome.test.sendMessage('ready');
