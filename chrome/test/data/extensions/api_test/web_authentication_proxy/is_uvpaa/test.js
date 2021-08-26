// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let receivedRequests = 0;
chrome.webAuthenticationProxy.onIsUvpaaRequest.addListener((requestId) => {
  receivedRequests++;
  chrome.test.assertTrue(receivedRequests <= 2);
  // We set the first request to false, and the second to true.
  let isUvpaa = receivedRequests == 2;
  chrome.webAuthenticationProxy.completeIsUvpaaRequest(
      {requestId, isUvpaa}, () => {
        chrome.test.assertNoLastError();
        chrome.test.succeed();
      });
});
chrome.test.sendMessage('ready');
