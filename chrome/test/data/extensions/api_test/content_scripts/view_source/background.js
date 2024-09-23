// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function noContentScriptsInViewSource() {

    chrome.runtime.onMessage.addListener(
      function(request, sender, sendResponse) {
        chrome.test.fail('Got a content script request from view source mode.');
    });

    // We rely on content scripts running at document_start to run before we
    // receive a tab update with 'complete' status.

    chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
      if (changeInfo.status === 'complete' &&
          tab.url.indexOf('test_file.html') != -1) {
        chrome.test.succeed();
      }
    });

    chrome.test.getConfig(function(config) {
      chrome.tabs.create({
        url: 'view-source:http://localhost:' + config.testServer.port +
             '/extensions/test_file.html'});
    });
  }
]);
