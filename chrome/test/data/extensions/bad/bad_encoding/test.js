// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testUrl = 'http://a.com:PORT/';

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);

  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(listener);

    chrome.test.runTests([
      function executeJavaScriptFileWithBadEncodingShouldFail() {
        chrome.tabs.executeScript(tabId, {
          file: 'bad_encoding/bad_encoding.js'
        }, chrome.test.callbackFail(
            'Could not load file \'bad_encoding/bad_encoding.js\' for ' +
            'content script. It isn\'t UTF-8 encoded.'));
      }
    ]);
  });

  chrome.tabs.create({ url: testUrl });
});

