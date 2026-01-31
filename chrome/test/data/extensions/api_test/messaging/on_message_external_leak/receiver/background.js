// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    (message, unusedSender, sendResponse) => {
      if (message === 'ping_leak') {
        // Explicitly return false. This should close the channel immediately.
        // Calling sendResponse() asynchronously should not be sent as a
        // response to the sender.
        setTimeout(() => {
          sendResponse({response: 'pong_leak'});
        }, 100);
        return false;
      }
    });
