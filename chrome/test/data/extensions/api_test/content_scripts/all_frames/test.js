// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We load a page that has one iframe
// So we should receive two "all_frames" messages, and one "top_frame_only"
// messages.

let numAllFramesMessages = 0;
let numTopFrameOnlyMessages = 0;

chrome.test.runTests([
  // Tests receiving a message from a content script and responding.
  function onMessage() {
    chrome.runtime.onMessage.addListener(function(
        request, sender, sendResponse) {
      if (request == 'all_frames') {
        numAllFramesMessages++;
      } else if (request == 'top_frame_only') {
        numTopFrameOnlyMessages++;
      } else {
        chrome.test.fail(`Unexpected request: ${JSON.stringify(request)}`);
      }

      if (numAllFramesMessages == 2 && numTopFrameOnlyMessages == 1) {
        chrome.test.succeed();
      }
    });
  },
]);

chrome.test.getConfig(function(config) {
  chrome.test.log('Creating tab...');

  const testUrl = `http://localhost:${config.testServer.port}` +
      '/extensions/test_file_with_iframe.html';

  chrome.tabs.create({url: testUrl});
});
