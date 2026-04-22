// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TEST_FILE_URL = 'http://127.0.0.1:PORT/extensions/test_file.html';

chrome.test.getConfig((config) => {
  let createdTabId = undefined;
  chrome.test.runTests([
    function createTab() {
      let testComplete = false;
      chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo) {
        if (!createdTabId || tabId != createdTabId ||
            changeInfo.status !== 'complete') {
          return;
        }
        chrome.tabs.onUpdated.removeListener(listener);
        if (!testComplete) {
          chrome.test.succeed();
        }
      });
      const testUrl = TEST_FILE_URL.replace(/PORT/, config.testServer.port);
      chrome.tabs.create({url: testUrl}, function(tab) {
        createdTabId = tab.id;
        if (tab.status === 'complete') {
          testComplete = true;
          chrome.test.succeed();
        }
        // Otherwise tabs.onUpdated will be called.
      });
    },
    function testExternalMessage() {
      chrome.tabs.sendMessage(createdTabId, 'worker->tab', function(response) {
        console.log(`response = ${response}`);
        chrome.test.assertEq('worker->tab->worker', response);
        chrome.test.succeed();
      });
    },
  ]);
});
