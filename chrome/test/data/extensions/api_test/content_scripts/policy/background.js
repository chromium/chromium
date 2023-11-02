// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;
var listenForever = chrome.test.listenForever;

var port;

function testUrl(domain) {
  return 'http://' + domain + ':' + port +
      '/extensions/test_file.html';
}

function error(domain) {
  return 'Cannot access contents of url "' + testUrl(domain) + '".' +
    ' Extension manifest must request permission to access this host.';
}

// Creates a new tab, navigated to the specified |domain|.
function createTestTab(domain, callback) {
  var createdTabId = -1;
  var done = listenForever(
      chrome.tabs.onUpdated,
      function(tabId, changeInfo, tab) {
    if (tabId == createdTabId && changeInfo.status != 'loading') {
      callback(tab);
      done();
    }
  });

  chrome.tabs.create({url: testUrl(domain)}, pass(function(tab) {
    createdTabId = tab.id;
  }));
}

chrome.test.getConfig(function(config) {
  port = config.testServer.port;
  chrome.test.runTests([

   // Make sure we can't inject a script into a policy blocked host.
   function policyBlocksInjection() {
    createTestTab('example.com', pass(function(tab) {
        chrome.tabs.executeScript(
            tab.id, {code: 'document.title = "success"'},
            callbackFail(
                'This page cannot be scripted due to ' +
                'an ExtensionsSettings policy.'));
        }));
   },
  ]);
});
