// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function listener(port) {
  port.onMessage.addListener((message) => {
    chrome.test.assertEq('background page', message);
    port.postMessage('content script');
  });
  chrome.runtime.onConnect.removeListener(listener);
});

chrome.runtime.onMessage.addListener(
    function listener(message, sender, sendResponse) {
  chrome.test.assertEq('async bounce', message);
  chrome.runtime.onMessage.removeListener(listener);
  // Respond asynchronously.
  setTimeout(() => { sendResponse('bounced'); }, 0);
  // When returning a result asynchronously, the listener must return true -
  // otherwise the channel is immediately closed.
  return true;
});

chrome.runtime.sendMessage('startFlow', function(response) {
  chrome.test.assertEq('started', response);
});
