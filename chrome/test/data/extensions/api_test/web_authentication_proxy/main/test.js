// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const ERROR_INVALID_SENDER = 'Error: Invalid sender';
const ERROR_INVALID_REQUEST_ID = 'Error: Invalid requestId';
const ERROR_ATTACH = 'Error: Another extension is already attached';
const ERROR_DETACH = 'Error: This extension is not currently attached';

//  A dummy JSON-encoded PublicKeyCredential for completeCreateRequest(). The
//  credential ID is base64url('test') = 'dGVzdA'.
const MAKE_CREDENTIAL_RESPONSE_JSON = `{
  "id": "dGVzdA",
  "rawId": "dGVzdA",
  "type": "public-key",
  "authenticatorAttachment": "cross-platform",
  "response": {
    "authenticatorData": "YoNLjwSfqzThzqXUg6At1bvcOxxscAyaoCRefuCi6I0BAAAAAA",
    "attestationObject": "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YVjE5FMp0DogaNHK9_e7CulU5rDmJZdF8y9IKfdQ8FAR-cJBAAAAAAAAAAAAAAAAAAAAAAAAAAAAQKnIoE6PUxtEEyfXqdBqSnQ6yPhGtof1L50MYa1JOtmfS5XD0Q7BzH-yYKi1D-BrdMMquwW8DBfzxAtUatWsSFGlAQIDJiABIVggqInVFbKi0k_Qd2WH9kK4hZnhXPjhWlRqTtQxoyros1IiWCCo9UskSZuzG14q_dREih7thij6Kj-YvwSd86USfrV5fA",
    "clientDataJSON": "eyJ0eXBlIjoid2ViYXV0aG4uY3JlYXRlIiwiY2hhbGxlbmdlIjoiZEdWemRBIiwib3JpZ2luIjoiaHR0cHM6Ly9leGFtcGxlLmNvbSIsImNyb3NzT3JpZ2luIjpmYWxzZX0",
    "publicKey": "dGVzdCBwdWJsaWMga2V5",
    "publicKeyAlgorithm": -7,
    "transports": ["usb"]
  },
  "clientExtensionResults": {}
}`;

//  A dummy JSON-encoded PublicKeyCredential for completeGetRequest(). The
//  credential ID is base64url('test') = 'dGVzdA'.
const GET_ASSERTION_RESPONSE_JSON = `{
  "id": "dGVzdA",
  "rawId": "dGVzdA",
  "type": "public-key",
  "authenticatorAttachment": "cross-platform",
  "response": {
    "authenticatorData": "YoNLjwSfqzThzqXUg6At1bvcOxxscAyaoCRefuCi6I0BAAAAAA",
    "clientDataJSON": "eyJ0eXBlIjoid2ViYXV0aG4uY3JlYXRlIiwiY2hhbGxlbmdlIjoiZEdWemRBIiwib3JpZ2luIjoiaHR0cHM6Ly9leGFtcGxlLmNvbSIsImNyb3NzT3JpZ2luIjpmYWxzZX0",
    "signature": "RTAhAoIAbL78xmC6MWDpx8-SN1FlNUXo2VcqwxDeNukhh5diAtpUINntpYqNyzR4JaEmhEBdgnHBv82bW-2LZj1l6CgzKABz",
    "userHandle": "dXNlcklk"
  },
  "clientExtensionResults": {}
}`;

const TEST_ERROR_MESSAGE = 'test error message';

// completeCreateRequest completes the request with the given request ID
// using the fake response in `MAKE_CREDENTIAL_RESPONSE_JSON`.
function completeCreateRequest(requestId, optErrorName) {
  let response = {
    requestId: requestId,
  };
  if (optErrorName) {
    response.error = {name: optErrorName, message: TEST_ERROR_MESSAGE};
  } else {
    response.responseJson = MAKE_CREDENTIAL_RESPONSE_JSON;
  }
  return chrome.webAuthenticationProxy.completeCreateRequest(response);
}

// completeGetRequest completes the request with the given request ID
// using the fake response in `GET_ASSERTION_RESPONSE_JSON`.
function completeGetRequest(requestId, optErrorName) {
  let response = {
    requestId: requestId,
  };
  if (optErrorName) {
    response.error = {name: optErrorName, message: TEST_ERROR_MESSAGE};
  } else {
    response.responseJson = GET_ASSERTION_RESPONSE_JSON;
  }
  return chrome.webAuthenticationProxy.completeGetRequest(response);
}

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
          await completeCreateRequest(request.requestId);
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
          await completeCreateRequest(request.requestId, nextError);
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
              completeCreateRequest(request.requestId), ERROR_INVALID_SENDER);
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
              completeCreateRequest(request.requestId),
              ERROR_INVALID_REQUEST_ID);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
    await chrome.webAuthenticationProxy.attach();
    chrome.test.sendMessage('ready');
  },
  async function getAssertion() {
    chrome.webAuthenticationProxy.onGetRequest.addListener(async (request) => {
      await completeGetRequest(request.requestId);
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
      await completeGetRequest(request.requestId, nextError);
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
          completeGetRequest(request.requestId), ERROR_INVALID_SENDER);
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
          completeGetRequest(request.requestId), ERROR_INVALID_REQUEST_ID);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
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
    chrome.test.notifyFail('No test found');
    return;
  }
  chrome.test.runTests(tests);
});
