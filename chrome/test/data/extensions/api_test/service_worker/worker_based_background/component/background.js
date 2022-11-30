// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function basicTest() {
    let config = await chrome.test.getConfig();
    let testUrl =
        `http://b.com:${config.testServer.port}/extensions/test_file.html`;
    chrome.tabs.onCreated.addListener((tab) => {
      if (tab.pendingUrl == testUrl)
        chrome.test.succeed();
    });
    chrome.tabs.create({url: testUrl});
  }
]);
