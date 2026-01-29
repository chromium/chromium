// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EVENT_MESSAGE_EXTENSION_STRING = 'Extension';

const kExtensionPath = 'extensions/api_test/webrequest/fencedFrames';

// Constants as functions, not to be called until after runTests.
function getURLHttpSimpleLoad() {
  return getServerURL(`${kExtensionPath}/main.html`, "a.test", "https");
}

function getURLFencedFrame() {
  return getServerURL(`${kExtensionPath}/frame.html`, "a.test", "https");
}

runTests([
  function testSendMessage() {
    var expectedEvents = [
      'onBeforeRequest',
      'onBeforeSendHeaders',
      'onHeadersReceived'
    ];

    // We need to capture the frame IDs dynamically because they can vary
    // depending on what other features are enabled in the browser, e.g
    // `kWebUIReloadButton` enables WebUI views which are created by the browser
    // at startup, and consume frame IDs before this test runs.
    let expectedParentFrameId = -1;
    let expectedFencedFrameId = -1;
    const getFrameIdsListener = (details) => {
      if (details.url.endsWith('iframe.html')) {
        expectedParentFrameId = details.frameId;
      } else if (details.url.endsWith('frame.html')) {
        expectedFencedFrameId = details.frameId;
      }
    };
    chrome.webRequest.onBeforeRequest.addListener(
          getFrameIdsListener,{urls: ['<all_urls>']});

    chrome.declarativeWebRequest.onMessage.addListener((details) => {
      if (EVENT_MESSAGE_EXTENSION_STRING != details.message) {
        chrome.test.fail('Invalid message: ' + details.message);
      }

      // Ensure that we have captured the frame IDs before asserting.
      // The `onMessage` event is triggered by the request for the fenced frame,
      // so we should have seen the `onBeforeRequest` for both the parent iframe
      // (which loads before) and the fenced frame (which is the current
      // request).
      chrome.test.assertTrue(
          expectedParentFrameId !== -1,
          'Parent frame ID should have been captured');
      chrome.test.assertTrue(
          expectedFencedFrameId !== -1,
          'Fenced frame ID should have been captured');

      chrome.test.assertEq(expectedFencedFrameId, details.frameId);
      chrome.test.assertEq(expectedParentFrameId, details.parentFrameId);
      chrome.test.assertEq('sub_frame', details.type);
      chrome.test.assertEq('fenced_frame', details.frameType);
      chrome.test.assertEq('active', details.documentLifecycle);
      chrome.test.assertTrue('parentDocumentId' in details);
      chrome.test.assertFalse('documentId' in details);
      chrome.test.assertEq(getURLFencedFrame(), details.url);
      chrome.test.assertEq(details.stage, expectedEvents.shift());
      if (expectedEvents.length == 0) {
        chrome.webRequest.onBeforeRequest.removeListener(getFrameIdsListener);
        chrome.test.succeed();
      }
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
