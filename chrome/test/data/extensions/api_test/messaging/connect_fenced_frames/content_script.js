// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function onConnect(port) {
  port.onMessage.addListener(function(msg) {
    if (msg.testPostMessage) {
      port.postMessage({success: true});
    } else if (msg.testDisconnect) {
      port.disconnect();
    }
  });
});

// For test onMessage.
function testSendMessageFromTab() {
  chrome.runtime.sendMessage({connected: true});
}

// The background script has to wait for the fenced frame to load so
// we start respond to the first test case here.
testSendMessageFromTab();
