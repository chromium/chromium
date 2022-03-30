// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let url;

window.onload = function() {
  chrome.test.runTests([
    async function init() {
      const config = await chrome.test.getConfig();
      const port = config.testServer.port;
      const id = chrome.runtime.id;
      const pageUrl =
          `http://chromium.org:${port}/extensions/favicon/test_file.html`;
      url = `chrome-extension://${id}/_favicon/?page_url=${pageUrl}`;
      chrome.test.succeed();
    },
    function image() {
      const img = document.createElement('img');
      document.body.appendChild(img);
      // TODO(solomonkinard): Currently, fetching favicons in this way isn't
      // supported. Adjust this test once it is. In the meantime, we verify that
      // the load fails as expected.
      img.onload = () => chrome.test.fail('Image loaded unexpectedly!');
      img.onerror = () => chrome.test.succeed();
      img.src = url;
    },
    async function path() {
      const response = await fetch(url);
      const text = await response.text();
      // TODO(solomonkinard): This case extension can be removed once the
      // _favicon endpoint returns the image bits. This test exists for now
      // to prove that the _favicon endpoint is being reached.
      chrome.test.assertEq('Favicon', text);
      chrome.test.succeed();
    }
  ]);
};
