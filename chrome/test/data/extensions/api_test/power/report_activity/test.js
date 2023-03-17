// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let wasIdle = false;
chrome.idle.onStateChanged.addListener((newState) => {
  // First state is set to be idle from the Chrome side of the test. We report
  // an activity once the state is idle.
  if (newState === 'idle') {
    wasIdle = true;
    chrome.test.sendMessage('idle');
  }
  // Only succeed a test if we go from "idle" to "active".
  else if (newState === 'active' && wasIdle) {
    chrome.test.succeed();
  }
});

chrome.test.sendMessage('ready');
