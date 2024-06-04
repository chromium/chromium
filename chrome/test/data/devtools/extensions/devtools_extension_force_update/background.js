// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
  if (message === 'devtools-panel-loaded') {
    chrome.test.sendMessage('extension devtools panel loaded');
  }
  if (message === 'ext-resource-loaded') {
    chrome.test.sendMessage('extension_resource.html loaded');
  }
});
