// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.test.runTests([async function test() {
    const config = await chrome.test.getConfig();
    const port = config.testServer.port;
    const id = chrome.runtime.id;
    const pageUrl =
        `http://www.example.com:${port}extensions/favicon/test_file.html`;
    const url = `chrome-extension://${id}/_favicon/?page_url=${pageUrl}`;
    fetch(url).then(() => chrome.test.fail()).catch(error => {
      chrome.test.assertEq('Failed to fetch', error.message);
      chrome.test.succeed();
    });
  }]);
};
