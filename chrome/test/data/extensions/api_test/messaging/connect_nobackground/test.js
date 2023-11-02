// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onConnect.addListener(function(port) {
  var succeed1 = chrome.test.callbackAdded();
  var succeed2 = chrome.test.callbackAdded();
  var succeed3 = chrome.test.callbackAdded();

  port.onMessage.addListener(function(msg) {
    chrome.test.log('port.onMessage was triggered.');
    chrome.test.assertEq('Hello from content script', msg);
    succeed1();
  });

  port.onDisconnect.addListener(function() {
    chrome.test.log('port.onDisconnect was triggered.');
    succeed2();
  });

  chrome.tabs.sendMessage(port.sender.tab.id, 'msg from tab', function(reply) {
    chrome.test.log('tab.sendMessage\'s response callback was invoked');
    chrome.test.assertEq('Reply here', reply);
    succeed3();
  });
});

chrome.test.getConfig(function(config) {
  var url = 'http://localhost:' + config.testServer.port +
      '/extensions/test_file.html?will_test_connect_and_sendMessage';
  // Content script will try to connect and trigger onConnect.
  chrome.tabs.create({
    url: url
  });
});
