// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(async (config) => {
  const receiverId = config.customArg;
  chrome.test.runTests([
    // Tests that an external message listener can respond asynchronously by
    // returning a promise.
    async function sendMessageExternalPromise() {
      const response = await chrome.runtime.sendMessage(receiverId, 'ping');
      chrome.test.assertEq({response: 'pong'}, response);
      chrome.test.succeed();
    },
  ]);
});
