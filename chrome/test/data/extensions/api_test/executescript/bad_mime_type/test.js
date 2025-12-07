// Copyright 2025 The Chromium Authors
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
      function executeJavaScriptFileWithBadMimeTypeShouldFail() {
        chrome.tabs.executeScript(tabId, {
          file: 'bad_mime_type.json'
        }, chrome.test.callbackFail(
            'Could not load file \'bad_mime_type.json\' for content script, ' +
            'content scripts can only be loaded from supported JavaScript ' +
            'files such as .js files.'));
      }
    ]);
  });

  chrome.tabs.create({ url: testUrl });
});
