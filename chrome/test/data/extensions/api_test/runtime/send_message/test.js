// Copyright 2021 The Chromium Authors
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

  function sendMessageWithCallbackAndNoResponse() {
    chrome.runtime.sendMessage('no_response', (response) => {
      chrome.test.assertLastError(
          'The message port closed before a response was received.');
      chrome.test.succeed();
    });
  },

  function sendMessageWithCallbackExpectingAsyncReply() {
    chrome.runtime.sendMessage('async_true', (response) => {
      chrome.test.assertEq('async_reply', response);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
    // After a short delay, send another message which should cause the stored
    // response for the previous sendMessage call to be called.
    setTimeout(chrome.runtime.sendMessage, 0, 'send_async_reply');
  },

  async function sendMessageWithPromise() {
    const response = await chrome.runtime.sendMessage('ping');
    chrome.test.assertEq('pong', response);
    chrome.test.succeed();
  },

  async function sendMessageWithPromiseAndNoResponse() {
    await chrome.runtime.sendMessage('no_response');
    chrome.test.succeed();
  },

  async function sendMessageWithPromiseExpectingAsyncReply() {
    chrome.runtime.sendMessage('async_true').then((response) => {
      chrome.test.assertEq('async_reply', response);
      chrome.test.succeed();
    });
    // After a short delay, send another message which should cause the stored
    // response for the previous sendMessage call to be called.
    setTimeout(chrome.runtime.sendMessage, 0, 'send_async_reply');
  },
]);
