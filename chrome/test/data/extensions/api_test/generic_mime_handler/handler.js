// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function getStreamInfoAndFetchStream() {
    const info = await chrome.mimeHandler.getStreamInfo();

    chrome.test.assertEq('application/pdf', info.mimeType);
    chrome.test.assertTrue(info.tabId >= 0);
    chrome.test.assertFalse(info.embedded);
    chrome.test.assertTrue(info.originalUrl.length > 0);
    chrome.test.assertTrue(info.streamUrl.length > 0);

    const response = await fetch(info.streamUrl);
    chrome.test.assertEq(200, response.status);
    const buffer = await response.arrayBuffer();
    const header = new Uint8Array(buffer, 0, 5);
    const magic = String.fromCharCode(...header);
    chrome.test.assertEq('%PDF-', magic);
    chrome.test.succeed();
  },
]);
