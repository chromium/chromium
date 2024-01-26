// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onInstalled.addListener((details) => {
  // Asynchronously send the message that the listener fired so that the event
  // is considered ack'd in the browser C++ code.
  setTimeout(() => {
    chrome.test.sendMessage('installed listener fired');
  }, 0);
});

chrome.webNavigation.onBeforeNavigate.addListener((details) => {
  // Asynchronously send the message that the listener fired so that the event
  // is considered ack'd in the browser C++ code.
  setTimeout(() => {
    chrome.test.sendMessage('listener fired');
  }, 0);
});
