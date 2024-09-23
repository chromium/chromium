// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We load a page that has one iframe
// So we should receive two "all_frames" messages, and one "top_frame_only"
// messages.

var num_all_frames_messages = 0;
var num_top_frame_only_messages = 0;

chrome.test.runTests([
  // Tests receiving a message from a content script and responding.
  function onMessage() {
    chrome.runtime.onMessage.addListener(
      function(request, sender, sendResponse) {
        if (request == "all_frames") {
          num_all_frames_messages++;
        } else if (request == "top_frame_only") {
          num_top_frame_only_messages++;
        } else {
          chrome.test.fail("Unexpected request: " + JSON.stringify(request));
        }

        if (num_all_frames_messages == 2 && num_top_frame_only_messages == 1) {
          chrome.test.succeed();
        }
      }
    );
  }
]);

chrome.test.getConfig(function(config) {
  chrome.test.log("Creating tab...");

  var test_url =
      "http://localhost:PORT/extensions/test_file_with_iframe.html"
          .replace(/PORT/, config.testServer.port);

  chrome.tabs.create({ url: test_url });
});
