// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  const relativePath = '/extensions/api_test/executescript/run_at/test.html';
  const testUrl = `http://b.com:${config.testServer.port}${relativePath}`;
  chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
    if (changeInfo.status != 'complete') {
      return;
    }
    chrome.tabs.onUpdated.removeListener(arguments.callee);

    chrome.test.runTests([
      function passUserGestureToExecutedScript() {
        chrome.runtime.onMessage.addListener(
            chrome.test.callbackPass(function(request, sender, sendResponse) {
              // The script executed by executeScript should run in a
              // user gesture context.
              chrome.test.assertTrue(request.user_gesture);
            }),
        );
        const code = 'chrome.runtime.sendMessage({' +
            '    user_gesture: chrome.test.isProcessingUserGesture()});';
        chrome.test.runWithUserGesture(function() {
          chrome.tabs.executeScript(tabId, {code: code});
        });
      },
    ]);
  });
  chrome.tabs.create({ url: testUrl });
});
