// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as util from '/_test_resources/api_test/web_authentication_proxy/util.js';

const ERROR_INVALID_SENDER = 'Error: Invalid sender';
const ERROR_INVALID_REQUEST_ID = 'Error: Invalid requestId';

let availableTests = [
  async function attachDetach() {
    await chrome.webAuthenticationProxy.attach();
    // Attaching the same extension again should be a no-op. (Attaching a
    // *different* extension would fail. This is tested in
    // WebAuthenticationProxyApiTest.AttachSecondExtension)
    await chrome.webAuthenticationProxy.attach();
    await chrome.webAuthenticationProxy.detach();
    // Similarly, detaching an unattached extension does nothing.
    await chrome.webAuthenticationProxy.detach();
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
    chrome.webAuthenticationProxy.onIsUvpaaRequest.addListener(
        async (request) => {
          await chrome.webAuthenticationProxy.detach();
          await chrome.test.assertPromiseRejects(
              chrome.webAuthenticationProxy.completeIsUvpaaRequest(
                  {requestId: request.requestId, isUvpaa: true}),
              ERROR_INVALID_SENDER);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });

    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function makeCredential() {
    chrome.webAuthenticationProxy.onCreateRequest.addListener(
        async (request) => {
          await util.completeCreateRequest(request.requestId);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function makeCredentialError() {
    let nextError;
    chrome.webAuthenticationProxy.onCreateRequest.addListener(
        async (request) => {
          chrome.test.assertTrue(nextError.length > 0);
          // The C++ side verifies that the passed in errorName matches the
          // error that  the WebAuthn client-side JS receives.
          await util.completeCreateRequest(request.requestId, nextError);
          chrome.test.assertNoLastError();
          nextError = await chrome.test.sendMessage('nextError');
          if (!nextError) {
            chrome.test.succeed();
          } else {
            chrome.test.sendMessage('nextRequest');
          }
        });
    await chrome.webAuthenticationProxy.attach();
    // The C++ side passes error names to be used in completeCreateRequest().
    nextError = await chrome.test.sendMessage('nextError');
    chrome.test.sendMessage('nextRequest');
  },
  async function makeCredentialResolvesOnDetach() {
    chrome.webAuthenticationProxy.onCreateRequest.addListener(
        async (request) => {
          await chrome.webAuthenticationProxy.detach();
          await chrome.test.assertPromiseRejects(
              util.completeCreateRequest(request.requestId),
              ERROR_INVALID_SENDER);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function makeCredentialCancel() {
    let canceled = false;
    let requestId;
    chrome.webAuthenticationProxy.onRequestCanceled.addListener(
        (canceledRequestId) => {
          chrome.test.assertFalse(canceled);
          canceled = true;
          chrome.test.assertTrue(canceledRequestId == requestId);
        });
    chrome.webAuthenticationProxy.onCreateRequest.addListener(
        async (request) => {
          chrome.test.assertFalse(canceled);
          requestId = request.requestId;
          await chrome.test.sendMessage('request');
          // Browser indicates the request completed, which means the cancel
          // handler should have been invoked.
          chrome.test.assertTrue(canceled);

          // Completing the canceled request should fail.
          await chrome.test.assertPromiseRejects(
              util.completeCreateRequest(request.requestId),
              ERROR_INVALID_REQUEST_ID);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function getAssertion() {
    chrome.webAuthenticationProxy.onGetRequest.addListener(async (request) => {
      await util.completeGetRequest(request.requestId);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function getAssertionError() {
    let nextError;
    chrome.webAuthenticationProxy.onGetRequest.addListener(async (request) => {
      chrome.test.assertTrue(nextError.length > 0);
      // The C++ side verifies that the passed in errorName matches the
      // error that  the WebAuthn client-side JS receives.
      await util.completeGetRequest(request.requestId, nextError);
      chrome.test.assertNoLastError();
      nextError = await chrome.test.sendMessage('nextError');
      if (!nextError) {
        chrome.test.succeed();
      } else {
        chrome.test.sendMessage('nextRequest');
      }
    });
    await chrome.webAuthenticationProxy.attach();
    // The C++ side passes error names to be used in completeGetRequest().
    nextError = await chrome.test.sendMessage('nextError');
    chrome.test.sendMessage('nextRequest');
  },
  async function getAssertionResolvesOnDetach() {
    chrome.webAuthenticationProxy.onGetRequest.addListener(async (request) => {
      await chrome.webAuthenticationProxy.detach();
      await chrome.test.assertPromiseRejects(
          util.completeGetRequest(request.requestId), ERROR_INVALID_SENDER);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function getAssertionCancel() {
    let canceled = false;
    let requestId;
    chrome.webAuthenticationProxy.onRequestCanceled.addListener(
        (canceledRequestId) => {
          chrome.test.assertFalse(canceled);
          canceled = true;
          chrome.test.assertTrue(canceledRequestId == requestId);
        });
    chrome.webAuthenticationProxy.onGetRequest.addListener(async (request) => {
      chrome.test.assertFalse(canceled);
      requestId = request.requestId;
      await chrome.test.sendMessage('request');
      // Browser indicates the request completed, which means the cancel
      // handler should have been invoked.
      chrome.test.assertTrue(canceled);

      // Completing the canceled request should fail.
      await chrome.test.assertPromiseRejects(
          util.completeGetRequest(request.requestId), ERROR_INVALID_REQUEST_ID);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function incognitoSpanning() {
    chrome.webAuthenticationProxy.onCreateRequest.addListener(
        async (request) => {
          await util.completeCreateRequest(request.requestId);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function policyBlockedHosts() {
    chrome.webAuthenticationProxy.onIsUvpaaRequest.addListener(
        async (requestInfo) => {
          await chrome.webAuthenticationProxy.completeIsUvpaaRequest(
              {requestId: requestInfo.requestId, isUvpaa: true});
          chrome.test.assertNoLastError();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
];

chrome.test.getConfig((config) => {
  const tests = availableTests.filter((t) => {
    return config.customArg == t.name;
  });
  if (tests.length == 0) {
    // Log because the C++ side might stall rather than notice the call to
    // notifyFail.
    console.error('No test found');
    chrome.test.notifyFail('No test found');
    return;
  }
  chrome.test.runTests(tests);
});
