// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EVENT_MESSAGE_EXTENSION_STRING = 'Extension';

const kExtensionPath = 'extensions/api_test/webrequest/fencedFrames';

// Constants as functions, not to be called until after runTests.
function getURLHttpSimpleLoad() {
  return getServerURL(`${kExtensionPath}/main.html`);
}

function getURLFencedFrame() {
  return getServerURL(`${kExtensionPath}/frame.html`);
}

runTests([
  function testSendMessage() {
    var expectedEvents = [
      'onBeforeRequest',
      'onBeforeSendHeaders',
      'onHeadersReceived'
    ];
    chrome.declarativeWebRequest.onMessage.addListener((details) => {
      if (EVENT_MESSAGE_EXTENSION_STRING != details.message) {
        chrome.test.fail('Invalid message: ' + details.message);
      }
      chrome.test.assertEq(5, details.frameId);
      chrome.test.assertEq(4, details.parentFrameId);
      chrome.test.assertEq('sub_frame', details.type);
      chrome.test.assertEq(getURLFencedFrame(), details.url);
      chrome.test.assertEq(details.stage, expectedEvents.shift());
      if (expectedEvents.length == 0)
        chrome.test.succeed();
    });

    var rule = {
      conditions: [
        new chrome.declarativeWebRequest.RequestMatcher(
            {url: {urlEquals: getURLFencedFrame()}}),
      ],
      actions: [
        new chrome.declarativeWebRequest.SendMessageToExtension(
            {message: EVENT_MESSAGE_EXTENSION_STRING}),
      ],
    };
    chrome.declarativeWebRequest.onRequest.addRules([rule], function() {
      chrome.tabs.create({url: getURLHttpSimpleLoad()});
    });
  }
]);
