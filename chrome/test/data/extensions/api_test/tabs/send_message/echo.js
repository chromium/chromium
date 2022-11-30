// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple onMessage listener that responds to "ping" messages with a "pong"
// message.
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  const response =
      request == 'ping' ? 'pong' : `Unexpected message: ${request}`;
  sendResponse(response);
});
