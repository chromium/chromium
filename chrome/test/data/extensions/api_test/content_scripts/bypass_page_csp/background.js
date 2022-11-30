// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  chrome.test.log("got connect");
  port.onMessage.addListener(function(msg) {
    chrome.test.log("got message: " + msg);
    if (msg)
      chrome.test.notifyPass();
    else
      chrome.test.notifyFail();
  });
});

chrome.test.getConfig(function(config) {
  chrome.test.log("Creating tab...");

  var test_url =
      "http://localhost:PORT/extensions/test_file_with_csp.html"
          .replace(/PORT/, config.testServer.port);

  chrome.tabs.create({ url: test_url });
});
