// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let prefetchRequestsSeen = new Map();

function wasRequestSeen(url) {
  const result = prefetchRequestsSeen.has(url);
  chrome.test.sendScriptResult(result);
}

function getInitiator(url) {
  const result = prefetchRequestsSeen.get(url).initiator;
  chrome.test.sendScriptResult(result);
}

function getRequestType(url) {
  const result = prefetchRequestsSeen.get(url).type;
  chrome.test.sendScriptResult(result);
}

chrome.webRequest.onBeforeSendHeaders.addListener(details => {
  for (const header of details.requestHeaders) {
    if (header.name === 'Sec-Purpose' && header.value === 'prefetch') {
      prefetchRequestsSeen.set(details.url, details);
    }
  }
}, { urls: ["<all_urls>"]}, ["requestHeaders"]);
