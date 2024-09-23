// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  // Tests receiving a request from a content script and responding.
  function onRequest() {
    chrome.runtime.onMessage.addListener(
      function(request, sender, sendResponse) {
        chrome.test.assertTrue(request.success);
        chrome.test.succeed();
      }
    );
  }
]);

chrome.test.getConfig(function(config) {
  chrome.test.log("Creating tab...");

  var test_url = "http://localhost:PORT/extensions/test_file.html"
      .replace(/PORT/, config.testServer.port);

  chrome.tabs.create({ url: test_url });
});
