// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function executeCodeInTab(tabId, callback) {
  chrome.tabs.executeScript(
      tabId,
      {code: "document.title = 'hi, I\\'m on ' + location;"},
      callback);
}

chrome.test.getConfig(function(config) {
  var path = "/extensions/test_file.txt";
  var urlC = "http://c.com:" + config.testServer.port + path;
  var urlB = "http://b.com:" + config.testServer.port + path;
  var testTabId;

  function onTabUpdated(tabId, changeInfo, tab) {
    if (testTabId == tab.id && tab.status == "complete") {
      chrome.tabs.onUpdated.removeListener(onTabUpdated);
      chrome.tabs.update(tabId, {url: urlB});
      executeCodeInTab(testTabId, function() {
        // Generally, the tab navigation hasn't happened by the time we execute
        // the script, so it's still showing a.com, where we don't have
        // permission to run it.
        if (chrome.runtime.lastError) {
          chrome.test.assertLastError(
              'Cannot access contents of url "' + urlC +
              '". Extension manifest must request permission to access this ' +
              'host.');
          chrome.test.notifyPass();
        } else {
          // If there were no errors, then the script did run, but it should
          // have run on on b.com (where we do have permission).
          chrome.tabs.get(tabId, function(tab) {
            chrome.test.assertTrue(
                tab.title.indexOf("hi, I'm on http://b.com:") == 0);
            chrome.test.notifyPass();
          });
        }
      });
    }
  }

  chrome.tabs.onUpdated.addListener(onTabUpdated);
  chrome.tabs.create({url: urlC}, function(tab) {
    testTabId = tab.id;
  });
});
