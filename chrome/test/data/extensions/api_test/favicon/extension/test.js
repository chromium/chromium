// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.test.runTests([async function favicon() {
    const img = document.createElement('img');
    document.body.appendChild(img);
    const config = await chrome.test.getConfig();
    const port = config.testServer.port;
    const id = chrome.runtime.id;
    const pageUrl =
        `http://chromium.org:{port}/extensions/favicon/test_file.html`;
    // TODO(solomonkinard): Currently, fetching favicons in this way isn't
    // supported. Adjust this test once it is. In the meantime, we verify that
    // the load fails as expected.
    img.onload = () => chrome.test.fail('Image loaded unexpectedly!');
    img.onerror = () => chrome.test.succeed();
    img.src = `chrome-extension://${id}/_favicon/?page_url=${pageUrl}`;
  }]);
};
