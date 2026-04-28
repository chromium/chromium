// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const RELATIVE_PATH = '/extensions/api_test/executescript/run_at/test.html';

chrome.test.getConfig(function(config) {
  const testUrl = `http://b.com:${config.testServer.port}${RELATIVE_PATH}`;
  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete') {
      return;
    }
    chrome.tabs.onUpdated.removeListener(arguments.callee);
    chrome.test.runTests([
      function executeAtStartShouldSucceed() {
        const scriptDetails = {};
        scriptDetails.code = `document.title = 'Injected';`;
        scriptDetails.runAt = 'document_start';
        chrome.tabs.executeScript(tabId, scriptDetails, function() {
          chrome.tabs.get(tabId, chrome.test.callbackPass(function(tab) {
            chrome.test.assertEq('Injected', tab.title);
          }));
        });
      },
    ]);
  });
  chrome.tabs.create({ url: testUrl });
});
