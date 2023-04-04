// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  // Store the sendResponse to prevent it from garbage collection, which
  // may result in closing the port.
  self.onMessageSendResponse = sendResponse;
  chrome.test.succeed();
  return true; // Response will be send asynchronously. To keep the port open.
});
