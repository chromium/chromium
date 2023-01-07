// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertTrue = chrome.test.assertTrue;

var rootNode = null;

function createTabAndWaitUntilLoaded(url, callback) {
  chrome.tabs.create({"url": url}, function(tab) {
    chrome.tabs.onUpdated.addListener(function(tabId, changeInfo) {
      if (tabId == tab.id && changeInfo.status == 'complete') {
        callback(tab);
      }
    });
  });
}

function setUpAndRunTests(allTests) {
  chrome.test.getConfig(function(config) {
    assertTrue('testServer' in config, 'Expected testServer in config');
    var url = 'http://a.com:PORT/index.html'
        .replace(/PORT/, config.testServer.port);

    function gotTree(returnedRootNode) {
      rootNode = returnedRootNode;
      if (rootNode.docLoaded) {
        chrome.test.runTests(allTests);
        return;
      }
      rootNode.addEventListener('loadComplete', function() {
        chrome.test.runTests(allTests);
      });
    }
    createTabAndWaitUntilLoaded(url, function(unused_tab) {
      chrome.automation.getTree(gotTree);
    });
  });
}
