// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.preflightHeadersReceivedCount = 0
self.preflightProxyAuthRequiredCount = 0;
self.preflightResponseStartedCount = 0;
self.preflightResponseStartedSuccessfullyCount = 0

chrome.webRequest.onHeadersReceived.addListener(function (details) {
  if (details.method === "OPTIONS") {
    ++self.preflightHeadersReceivedCount;
    if (details.statusCode == 407) {
      ++self.preflightProxyAuthRequiredCount;
    }
  }
}, { urls: ['http://cors.test/*'] }, ["extraHeaders"]);

chrome.webRequest.onResponseStarted.addListener(function (details) {
  if (details.method === "OPTIONS") {
    ++self.preflightResponseStartedCount;
    if (details.statusCode == 204) {
      ++self.preflightResponseStartedSuccessfullyCount;
    }
  }
}, { urls: ['http://cors.test/*'] }, ["extraHeaders"]);

chrome.webRequest.onCompleted.addListener(function (details) {
  if (details.method === "OPTIONS" && details.statusCode == 204) {
    chrome.test.sendMessage('cors-preflight-succeeded');
  }
}, { urls: ['http://cors.test/*'] }, ["extraHeaders"]);

chrome.test.sendMessage('ready');
