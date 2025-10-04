// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.sendMessage('ready', (message) => {
  if (message === 'go') {
    chrome.runtime.sendMessage('ping');
    return;
  }
  console.error(`Unknown message ${message}`);
});
