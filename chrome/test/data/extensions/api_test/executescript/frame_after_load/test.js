// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let baseUrl =
    'http://a.com:PORT/extensions/api_test/executescript/frame_after_load/';

chrome.tabs.onUpdated.addListener(function(tabId, changeInfo, tab) {
  if (changeInfo.status != 'complete') {
    return;
  }

  chrome.test.runTests([
    function() {
      // Tests that we can still execute scripts after a frame has loaded after
      // the main document has completed.
      const injectFrameCode = `let frame = document.createElement('iframe');
          frame.src = '${baseUrl}inner.html';
          frame.onload = function() {
            chrome.runtime.connect().postMessage('loaded');
          };
          document.body.appendChild(frame);`;
      const postFrameCode = `chrome.runtime.connect().postMessage('done');`;

      chrome.runtime.onConnect.addListener(function(port) {
        port.onMessage.addListener(function(data) {
          if (data == 'loaded') {
            chrome.tabs.executeScript(tabId, {code: postFrameCode});
          } else if (data == 'done') {
            chrome.test.succeed();
          }
        });
      });
      chrome.tabs.executeScript(tabId, {code: injectFrameCode});
    },
  ]);
});

chrome.test.getConfig(function(config) {
  baseUrl = baseUrl.replace(/PORT/, config.testServer.port);
  chrome.tabs.create({ url: `${baseUrl}outer.html` });
});
