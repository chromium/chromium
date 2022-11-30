// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(config => {
  chrome.test.runTests([function testRemoteScript() {
    let port = config.testServer.port;
    let script = document.createElement('script');
    script.src = `http://google.com:${port}/script.js`;
    script.onload = () => {
      chrome.test.assertEq('New Title', document.title);
      chrome.test.succeed();
    };
    script.onerror = () => {
      chrome.test.fail('Script load failed unexpectedly');
    };
    document.body.appendChild(script);
  }]);
});
