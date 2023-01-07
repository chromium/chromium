// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var asyncResponseCallback;

// A simple onMessage listener we can send a "ping" message to and get a "pong"
// message back.
chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  switch (request) {
    case 'ping':
      // Simple case of receiving a message and replying with a response.
      sendResponse('pong');
      break;
    case 'no_response':
      // Case for receiving a message and not replying or indicating there will
      // be an asynchronous response.
      break;
    case 'async_true':
      // Case where true is returned to indicate we intend to respond
      // asynchronously.
      asyncResponseCallback = sendResponse;
      return true;
    case 'send_async_reply':
      asyncResponseCallback('async_reply');
      break;
    default:
      sendResponse('Unexpected message: ${request}');
  }
});
