// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  port.onMessage.addListener(function(msg) {
    if (msg.testSendMessageToFrame) {
      // page.js created this frame with an unique digit starting at 0.
      // This number is used in test.js to identify messages from this frame.
      var test_id = location.search.slice(-1);
      port.postMessage('from_' + test_id);
    } else if (msg.testConnectChildFrameAndNavigate) {
      location.search = '?testConnectChildFrameAndNavigateDone';
    }
  });
});

// continuation of testSendMessageFromFrame()
if (location.search.lastIndexOf('?testSendMessageFromFrame', 0) === 0 ||
    location.search.lastIndexOf('?testSendMessageFromSandboxedFrame', 0) ===
        0) {
  chrome.runtime.sendMessage({frameUrl: location.href});
} else if (location.search === '?testConnectChildFrameAndNavigateSetup') {
  // continuation of connectChildFrameAndNavigate() 1/2
  chrome.runtime.sendMessage('testConnectChildFrameAndNavigateSetupDone');
}
