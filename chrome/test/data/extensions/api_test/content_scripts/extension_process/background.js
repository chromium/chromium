// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let numPings = 0;
let baseUrl;

chrome.runtime.onMessage.addListener(function(data) {
  if (data != 'ping') {
    chrome.test.fail(`Unexpected request: ${JSON.stringify(data)}`);
  }

  if (++numPings == 2) {
    // Navigate the iframe to another page and check that the content script is
    // also injected into that page.
    document.querySelector('iframe').src = `${baseUrl}test_file_with_body.html`;
  } else if (numPings == 3) {
    chrome.test.notifyPass();
  }
});

chrome.test.getConfig(function(config) {
  baseUrl = `http://localhost:${config.testServer.port}/extensions/`;
  const testFileUrl = `${baseUrl}test_file.html`;

  // Add a window.
  const w = window.open(testFileUrl);

  // Add an iframe.
  const iframe = document.createElement('iframe');
  iframe.src = testFileUrl;
  document.getElementById('iframeContainer').appendChild(iframe);
});
