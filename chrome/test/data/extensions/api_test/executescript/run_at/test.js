// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var relativePath =
    '/extensions/api_test/executescript/run_at/test.html';
var testUrl = 'http://b.com:PORT' + relativePath;

chrome.test.getConfig(function(config) {
  testUrl = testUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete')
      return;
    chrome.tabs.onUpdated.removeListener(arguments.callee);
    chrome.test.runTests([
      function executeAtStartShouldSucceed() {
        var scriptDetails = {};
        scriptDetails.code = "document.title = 'Injected';";
        scriptDetails.runAt = "document_start";
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
