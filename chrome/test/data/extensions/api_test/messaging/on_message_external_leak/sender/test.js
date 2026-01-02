// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(async (config) => {
  const receiverId = config.customArg;
  chrome.test.runTests([
    // Tests that an external message channel closes if the listener returns
    // false and doesn't reply synchronously.
    function sendMessageExternalNoChannelLeak() {
      chrome.runtime.sendMessage(receiverId, 'ping_leak', (response) => {
        chrome.test.assertLastError(
            'The message port closed before a response was received.');
        chrome.test.succeed();
      });
    },
  ]);
});
