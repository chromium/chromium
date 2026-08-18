// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const inServiceWorker = 'ServiceWorkerGlobalScope' in self;
const extensionUrl = chrome.runtime.getURL('/');

function checkMessageUrl(url) {
  if (inServiceWorker) {
    chrome.test.assertEq(extensionUrl, url);
  } else {
    chrome.test.assertEq(window.location.origin + '/', url);
  }
}

chrome.test.getConfig(async function(config) {
  const platformInfo = await chrome.runtime.getPlatformInfo();
  const isAndroid = platformInfo.os === 'android';
  const APP_NAME = isAndroid ? 'org.chromium.chrome.tests.support' :
                               'com.google.chrome.test.echo';

  // Desktop: attempts to message a nonexistent native host.
  // Android: attempts to message a nonexistent external app.
  function invalidHostName() {
    const invalidHost = 'not.installed.app';
    const expectedError = isAndroid ?
        `Unable to connect to ${invalidHost}.` :
        'Specified native messaging host not found.';

    const message = {text: 'Hello!'};
    chrome.runtime.sendNativeMessage(
        invalidHost, message,
        chrome.test.callbackFail(expectedError, function(response) {
          chrome.test.assertEq(undefined, response);
        }));
  }

  // Desktop: attempts to message an existing native host but the executable
  // path the host points to does not exist.
  // Android: the external app exists but cannot receive extension messages.
  function nonexistentHost() {
    const nonexistentHost = 'com.google.chrome.test.host_binary_missing';
    const expectedError = isAndroid ?
        `Unable to connect to ${nonexistentHost}.` :
        'Specified native messaging host not found.';

    const message = {text: 'Hello!'};
    chrome.runtime.sendNativeMessage(
        nonexistentHost, message,
        chrome.test.callbackFail(expectedError, function(response) {
          chrome.test.assertEq(undefined, response);
        }));
  }

  function sendMessageWithCallback() {
    const message = {text: 'Hi there!', number: 3};
    chrome.runtime.sendNativeMessage(
        APP_NAME, message, chrome.test.callbackPass(function(response) {
          chrome.test.assertEq(1, response.id);
          chrome.test.assertEq(message, response.echo);
          checkMessageUrl(response.caller_url);
        }));
  }

  // The goal of this test is just not to crash.
  function sendMessageWithoutCallback() {
    const message = {text: 'Hi there!', number: 3};
    chrome.runtime.sendNativeMessage(APP_NAME, message);
    chrome.test.succeed();  // Mission Complete
  }

  function bigMessage() {
    // Create a special message for which the test host must try sending a
    // message that is bigger than the limit.
    const message = {bigMessageTest: true};
    chrome.runtime.sendNativeMessage(
        APP_NAME, message,
        chrome.test.callbackFail(
            'Error when communicating with the native messaging host.',
            function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  }

  function invalidMessage() {
    // Create a special message that's not valid JSON.
    const message = {sendInvalidResponse: true};
    chrome.runtime.sendNativeMessage(
        APP_NAME, message,
        chrome.test.callbackFail(
            'The sender sent an invalid JSON message; message ignored.',
            function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  }

  if (isAndroid) {
    // TODO(crbug.com/515159909): Add `bigMessage` to the list of tests after
    // larger messages are supported for Android.
    chrome.test.runTests([
      invalidHostName,
      nonexistentHost,
      sendMessageWithCallback,
      sendMessageWithoutCallback,
      invalidMessage,
    ]);
    return;
  }

  chrome.test.runTests([
    invalidHostName,
    nonexistentHost,
    sendMessageWithCallback,
    sendMessageWithoutCallback,
    bigMessage,
    invalidMessage,
  ]);
});
