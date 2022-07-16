// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ERROR_ATTACH = 'Error: Another extension is already attached';
const ERROR_DETACH = 'Error: This extension is not currently attached';

let availableTests = [
  async function attachDetach() {
    await chrome.webAuthenticationProxy.attach();
    await chrome.test.assertPromiseRejects(
        chrome.webAuthenticationProxy.attach(), ERROR_ATTACH);
    await chrome.webAuthenticationProxy.detach();
    await chrome.test.assertPromiseRejects(
        chrome.webAuthenticationProxy.detach(), ERROR_DETACH);
    chrome.test.succeed();
  },
  async function attachReload() {
    // This test will be run twice by reloading the extension.
    await chrome.webAuthenticationProxy.attach();
    chrome.test.succeed();
  },
  async function attachSecondExtension() {
    // The first attempt to attach should succeed.
    await chrome.webAuthenticationProxy.attach();
    await chrome.test.sendMessage('attached');
    // The browser loaded a second extension. Detach so that it can attach.
    await chrome.webAuthenticationProxy.detach();
    await chrome.test.sendMessage('detached');
    // Browser unloaded the second extension. Reattaching should
    // succeed.
    await chrome.webAuthenticationProxy.attach();
    chrome.test.succeed();
  },
  async function isUvpaa() {
    let receivedRequests = 0;
    chrome.webAuthenticationProxy.onIsUvpaaRequest.addListener(
        async (requestInfo) => {
          receivedRequests++;
          chrome.test.assertTrue(receivedRequests <= 2);
          // We set the first request to false, and the second to true.
          let isUvpaa = receivedRequests == 2;
          await chrome.webAuthenticationProxy.completeIsUvpaaRequest(
              {requestId: requestInfo.requestId, isUvpaa});
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function isUvpaaNotAttached() {
    let eventHandlerCalled = false;
    chrome.webAuthenticationProxy.onIsUvpaaRequest.addListener(
        (requestInfo) => {
          eventHandlerCalled = true;
        });
    await chrome.test.sendMessage('ready');
    chrome.test.assertFalse(eventHandlerCalled);
    chrome.test.succeed();
  },
  async function isUvpaaResolvesOnDetach() {
    let isUvpaaEventReceived = false;
    chrome.webAuthenticationProxy.onIsUvpaaRequest.addListener(
        (requestInfo) => {
          isUvpaaEventReceived = true;
          chrome.webAuthenticationProxy.detach();
        });

    await chrome.webAuthenticationProxy.attach();
    await chrome.test.sendMessage('ready');
    // The browser side signaled that the isUvpaa request has resolved.
    chrome.test.assertTrue(isUvpaaEventReceived);
    chrome.test.succeed();
  },
];

chrome.test.getConfig((config) => {
  const tests = availableTests.filter((t) => {
    return config.customArg == t.name;
  });
  if (tests.length == 0) {
    chrome.test.notifyFail('No test found');
    return;
  }
  chrome.test.runTests(tests);
});
