// Copyright 2014 The Chromium Authors
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
      function passUserGestureToExecutedScript() {
        chrome.runtime.onMessage.addListener(
            chrome.test.callbackPass(function(request, sender, sendResponse) {
                // The script executed by executeScript should run in a
                // user gesture context.
                chrome.test.assertTrue(request.user_gesture);
            })
        );
        var code = "chrome.runtime.sendMessage({" +
                   "    user_gesture: chrome.test.isProcessingUserGesture()});"
        chrome.test.runWithUserGesture(function() {
          chrome.tabs.executeScript(tabId, {code: code});
        });
      }
    ]);
  });
  chrome.tabs.create({ url: testUrl });
});
