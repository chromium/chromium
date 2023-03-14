// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var listenOnce = chrome.test.listenOnce;
var listenForever = chrome.test.listenForever;

// Keep track of the tab that we're running tests in, for simplicity.
var testTab = null;

function compareSenders(expected, actual) {
  // documentId is a unique ID so we can't assume anything about it, just
  // that it is provided.
  chrome.test.assertNe(undefined, actual.documentId);
  chrome.test.assertEq('active', actual.documentLifecycle);
  chrome.test.assertEq(expected.frameId, actual.frameId);
  chrome.test.assertEq(expected.url, actual.url);
  chrome.test.assertEq(serverOrigin, actual.origin);
  chrome.test.assertEq(chrome.runtime.id, actual.id);
}

function createExpectedSender(frameId, url) {
  return {frameId: frameId, url: url};
}

var serverOrigin;
var serverURL;

var tests = [
  // Tests that sendMessage from the fenced frame works.
  async function sendMessageFromTab() {
    const url = serverURL + 'main.html';

    // Because there is no way to observe that a fenced frame has loaded we
    // need to first wait for a message from the fenced frame indicating it
    // has loaded. To avoid racy behavior we first bind a listener, then load
    // the tab and wait for that message.
    let actualSender;
    let messagePromise = new Promise((resolve) => {
      chrome.runtime.onMessage.addListener(function messageListener(message,
                                                                    sender) {
        chrome.test.assertEq(message.connected, true);
        chrome.runtime.onMessage.removeListener(messageListener);
        actualSender = sender;
        resolve();
      });
    });

    // This tab will be used for the other tests as well.
    testTab = await new Promise(function(resolve, reject) {
      chrome.tabs.create({url: url}, (value) => {
        resolve(value);
      });
    });
    await messagePromise;
    expectedSender = createExpectedSender(5, serverURL + 'fenced_frame.html');
    compareSenders(expectedSender, actualSender);
    chrome.test.succeed();
  },

  // Tests that postMessage to the fenced frame and its response works.
  function postMessage() {
    var port = chrome.tabs.connect(testTab.id);
    port.postMessage({testPostMessage: true});
    listenOnce(port.onMessage, function(msg) {
      port.disconnect();
    });
  },

  // Tests that we get the disconnect event when the tab disconnect.
  function disconnect() {
    var port = chrome.tabs.connect(testTab.id);
    port.postMessage({testDisconnect: true});
    listenOnce(port.onDisconnect, function() {});
  }
];

chrome.test.getConfig(async (config) => {
  serverOrigin = `http://localhost:${config.testServer.port}`;
  serverURL = serverOrigin + '/extensions/api_test/messaging/'
                           + 'connect_fenced_frames/';
  chrome.test.runTests(tests);
});
