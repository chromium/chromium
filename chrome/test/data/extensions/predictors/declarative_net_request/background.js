// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let requestsBlocked = new Set();

async function waitUntilRequestBlocked(url) {
  if (requestsBlocked.has(url)) {
    chrome.test.sendScriptResult(true);
    return;
  }
  await new Promise(resolve => {
    chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(info => {
      if (info.request.url === url) {
        resolve();
      }
    });
  });
  chrome.test.sendScriptResult(true);
}

chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(info => {
  requestsBlocked.add(info.request.url);
});
