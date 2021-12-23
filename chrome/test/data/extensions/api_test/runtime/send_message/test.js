// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function sendMessageWithCallback() {
    chrome.runtime.sendMessage('ping', (response) => {
      chrome.test.assertEq('pong', response);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  async function sendMessageWithPromise() {
    const response = await chrome.runtime.sendMessage('ping');
    chrome.test.assertEq('pong', response);
    chrome.test.succeed();
  }
]);
