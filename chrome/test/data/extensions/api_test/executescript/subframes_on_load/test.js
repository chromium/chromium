// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  // If we want to inject into subframes using execute script, we need to wait
  // for the tab to finish loading. Otherwise, the frames that would be injected
  // into don't exist.
  if (changeInfo.status != 'complete')
    return;

  chrome.test.runTests([
    function() {
      var injectFrameCode = 'chrome.runtime.sendMessage("ping");';
      var pings = 0;
      chrome.runtime.onMessage.addListener(
           function(request, sender, sendResponse) {
        chrome.test.assertEq(request, "ping");
        // Wait for two pings - the main frame and the iframe.
        if (++pings == 2)
          chrome.test.succeed();
      });
      chrome.tabs.executeScript(
          tabId,
          {code: injectFrameCode, allFrames: true, runAt: 'document_idle'});
    }
  ]);
});

chrome.test.getConfig(function(config) {
  var url = ('http://a.com:PORT/extensions/api_test/executescript/' +
             'subframes_on_load/outer.html').replace(/PORT/,
                                                     config.testServer.port);
  chrome.tabs.create({url: url});
});
