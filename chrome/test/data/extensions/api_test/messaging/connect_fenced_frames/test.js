// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const listenOnce = chrome.test.listenOnce;
const listenForever = chrome.test.listenForever;

// Keep track of the tab that we're running tests in, for simplicity.
let testTab = null;

let serverOrigin;
let serverURL;

const tests = [
  // Tests that sendMessage from the fenced frame works.
  async function sendMessageFromTab() {
    const url = `${serverURL}main.html`;

    // Because there is no way to observe that a fenced frame has loaded we
    // need to first wait for a message from the fenced frame indicating it
    // has loaded. To avoid racy behavior we first bind a listener, then load
    // the tab and wait for that message.
    let actualSender;
    const messagePromise = new Promise((resolve) => {
      chrome.runtime.onMessage.addListener(
          function messageListener(message, sender) {
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
    // documentId / frameId are unique IDs so we can't assume anything about
    // it, it's also possible that they are changed just that it is provided.
    chrome.test.assertNe(undefined, actualSender.documentId);
    chrome.test.assertNe(undefined, actualSender.frameId);
    chrome.test.assertEq('active', actualSender.documentLifecycle);
    chrome.test.assertEq(`${serverURL}fenced_frame.html`, actualSender.url);
    chrome.test.assertEq(serverOrigin, actualSender.origin);
    chrome.test.assertEq(chrome.runtime.id, actualSender.id);
    chrome.test.succeed();
  },

  // Tests that postMessage to the fenced frame and its response works.
  function postMessage() {
    const port = chrome.tabs.connect(testTab.id);
    port.postMessage({testPostMessage: true});
    listenOnce(port.onMessage, function(msg) {
      port.disconnect();
    });
  },

  // Tests that we get the disconnect event when the tab disconnect.
  function disconnect() {
    const port = chrome.tabs.connect(testTab.id);
    port.postMessage({testDisconnect: true});
    listenOnce(port.onDisconnect, function() {});
  },
];

chrome.test.getConfig(async (config) => {
  serverOrigin = `http://localhost:${config.testServer.port}`;
  serverURL = `${serverOrigin}/extensions/api_test/messaging/` +
      'connect_fenced_frames/';
  chrome.test.runTests(tests);
});
