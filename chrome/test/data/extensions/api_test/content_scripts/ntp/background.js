// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The NTP may take different forms, depending on the OS.
var newTabUrls = [
  'chrome://newtab/',
  'chrome-native://newtab/',
];

function testExecuteScriptInNewTab() {
  // Create a new tab to chrome://newtab and wait for the loading to complete.
  // Then, try to inject a script into that tab. The injection should fail.
  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    if (!newTabUrls.includes(tab.url) || changeInfo.status != 'complete') {
      return;
    }
    chrome.tabs.onUpdated.removeListener(listener);
    chrome.tabs.executeScript(tab.id, {file: 'script.js'}, function() {
      chrome.test.assertTrue(!!chrome.runtime.lastError);
      const lastErrorMessage = chrome.runtime.lastError.message;
      chrome.test.assertTrue(
          lastErrorMessage.indexOf('Cannot access contents of') != -1 ||
              lastErrorMessage.indexOf('Cannot access a chrome:// URL') != -1,
          lastErrorMessage);
      chrome.test.succeed();
    });
  });
  chrome.tabs.create({url: 'chrome://newtab'});
}

chrome.test.sendMessage('ready', function() {
  chrome.test.runTests([testExecuteScriptInNewTab]);
});
