// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(port => {
  port.onMessage.addListener(msg => {
    chrome.test.log("got message: " + msg);
    if (msg === true)
      chrome.test.notifyPass();
    else
      chrome.test.notifyFail("Expected message 'true', but got: " + msg);
  });
});

chrome.test.getConfig(config => {
  chrome.test.log("Creating tab...");

  var testUrl =
      "http://localhost:PORT/extensions/test_file_with_trusted_types.html"
          .replace(/PORT/, config.testServer.port);

  chrome.tabs.create({ url: testUrl });
});
