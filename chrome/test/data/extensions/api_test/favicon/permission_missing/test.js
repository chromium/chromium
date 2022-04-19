// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.test.runTests([async function test() {
    const config = await chrome.test.getConfig();
    const port = config.testServer.port;
    const id = chrome.runtime.id;
    const pageUrl = `http://chromium.org:{port}/extensions/favicon/test.html`;
    const url = `chrome-extension://${id}/_favicon/?page_url=${pageUrl}`;
    fetch(url)
        .then(res => {
          // TODO(solomonkinard): A 404 is expected when the favicon permission
          // is missing.
          chrome.test.assertEq(200, res.status);
          return res.text();
        })
        .then(text => {
          chrome.test.assertEq('', text);
          chrome.test.succeed();
        });
  }]);
};
