// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let num_listener_called = 0;

chrome.runtime.onInstalled.addListener((details) => {
  // Asynchronously send the message that the listener fired so that the event
  // is considered ack'd in the browser C++ code.
  setTimeout(() => {
    chrome.test.sendMessage('installed listener fired');
  }, 0);
});

chrome.webNavigation.onBeforeNavigate.addListener((details) => {
  num_listener_called++;

  // This listener should be run 3 times during the test, running more than 3
  // times will be caught by other test expectations.
  if (num_listener_called == 3) {
    // Asynchronously send the message that the listener fired so that the event
    // is considered ack'd in the browser C++ code.
    setTimeout(() => {
      chrome.test.sendMessage('listener fired three times');
    }, 0);
  }
});
