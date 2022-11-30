// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = chrome.runtime.connect();
port.onDisconnect.addListener(function() {
  chrome.test.fail('onDisconnect should not be triggered because the ' +
     'background page exists and the tab should have been crashed');
});

var ref;
chrome.runtime.onMessage.addListener(function(msg, sender, sendResponse) {
  chrome.test.assertEq('Rob says hi', msg);
  port.postMessage('is_ready_to_crash');
  // Keep the callback around to avoid test flakiness due to GC.
  ref = sendResponse;

  // Keep the port open - do not send a response.
  return true;
});
