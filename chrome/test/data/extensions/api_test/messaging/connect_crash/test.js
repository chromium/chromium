// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  var is_ready_to_crash = false;
  var succeed1 = chrome.test.callbackAdded();
  var succeed2 = chrome.test.callbackAdded();

  port.onMessage.addListener(function(msg) {
    chrome.test.assertEq('is_ready_to_crash', msg);
    is_ready_to_crash = true;
    chrome.test.sendMessage('ready_to_crash');
    // Now the browser test should kill the tab, and the port should be closed.
  });
  port.onDisconnect.addListener(function() {
    chrome.test.log('port.onDisconnect was triggered.');
    chrome.test.assertTrue(is_ready_to_crash);
    succeed1();
  });

  chrome.tabs.sendMessage(port.sender.tab.id, 'Rob says hi', function() {
    chrome.test.log('tab.sendMessage\'s response callback was invoked');
    chrome.test.assertLastError(
        'A listener indicated an asynchronous response by returning true, ' +
        'but the message channel closed before a response was received');
    succeed2();
  });
});
