// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let storageComplete = undefined;
let isUsingStorage = false;

// Waits for any pending load to complete to avoid raciness in the test.
async function flushStorage() {
  console.assert(!storageComplete);
  if (!isUsingStorage)
    return;
  await new Promise((resolve) => {
    storageComplete = resolve;
  });
  storageComplete = undefined;
}

// Increments a counter storing the number of seen events.
function beforeRequestListener() {
  isUsingStorage = true;
  chrome.storage.local.get(
      {requestCount: 0},
      (result) => {
        let currentCount = result.requestCount;
        chrome.test.assertTrue(typeof currentCount == 'number');
        chrome.test.assertTrue(currentCount >= 0);
        ++currentCount;
        chrome.storage.local.set(
            {requestCount: currentCount},
            () => {
              isUsingStorage = false;
              if (storageComplete)
                storageComplete();
            });
      });
}

chrome.webRequest.onBeforeRequest.addListener(
    beforeRequestListener,
    {urls: ["*://example.com/*"], types: ['main_frame']});

chrome.test.sendMessage('ready');
