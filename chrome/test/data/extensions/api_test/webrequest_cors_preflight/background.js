// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.preflightHeadersReceivedCount = 0
window.preflightProxyAuthRequiredCount = 0;
window.preflightResponseStartedCount = 0;
window.preflightResponseStartedSuccessfullyCount = 0

chrome.webRequest.onHeadersReceived.addListener(function (details) {
  if (details.method === "OPTIONS") {
    ++window.preflightHeadersReceivedCount;
    if (details.statusCode == 407) {
      ++window.preflightProxyAuthRequiredCount;
    }
  }
}, { urls: ['http://cors.test/*'] }, ["extraHeaders"]);

chrome.webRequest.onResponseStarted.addListener(function (details) {
  if (details.method === "OPTIONS") {
    ++window.preflightResponseStartedCount;
    if (details.statusCode == 204) {
      ++window.preflightResponseStartedSuccessfullyCount;
    }
  }
}, { urls: ['http://cors.test/*'] }, ["extraHeaders"]);

chrome.webRequest.onCompleted.addListener(function (details) {
  if (details.method === "OPTIONS" && details.statusCode == 204) {
    chrome.test.sendMessage('cors-preflight-succeeded');
  }
}, { urls: ['http://cors.test/*'] }, ["extraHeaders"]);

chrome.test.sendMessage('ready');
