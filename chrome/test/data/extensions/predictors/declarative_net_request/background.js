// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let requestsBlocked = new Set();

function wasRequestBlocked(url) {
  const result = requestsBlocked.has(url);
  chrome.test.sendScriptResult(result);
}

chrome.declarativeNetRequest.onRuleMatchedDebug.addListener(info => {
  requestsBlocked.add(info.request.url);
});
