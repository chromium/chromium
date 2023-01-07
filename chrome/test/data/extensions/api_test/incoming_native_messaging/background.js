// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var NATIVE_APP_NAME = 'com.google.chrome.test.initiator';
var MESSAGE_TO_SEND = {request: 'foo'};
var EXPECTED_RESPONSE_MESSAGE = {response: 'bar'};

function assertValidNativeMessageSender(sender) {
  chrome.test.assertEq(undefined, sender.id);
  chrome.test.assertEq(NATIVE_APP_NAME, sender.nativeApplication);
  chrome.test.assertEq(undefined, sender.tlsChannelId);
  chrome.test.assertEq(undefined, sender.tab);
  chrome.test.assertEq(undefined, sender.frameId);
  chrome.test.assertEq(undefined, sender.url);
}

chrome.test.runTests([
  // Tests that the extension receives the onConnectNative event from the native
  // application, can send a message to it and receive a response back.
  function onConnectNativeTest() {
    chrome.runtime.onConnectNative.addListener(function(port) {
      assertValidNativeMessageSender(port.sender);
      port.postMessage(MESSAGE_TO_SEND);
      port.onMessage.addListener(function(message) {
        chrome.test.assertEq(EXPECTED_RESPONSE_MESSAGE, message);
        chrome.test.succeed();
      });
    });
  }
]);
