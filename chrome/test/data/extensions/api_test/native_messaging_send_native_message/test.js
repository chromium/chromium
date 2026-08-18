// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this file is copy-pasted from /api_test/native_messaging/lazy/test.js
// and then adapted to use MV3 and test promises.

const extensionUrl = chrome.runtime.getURL('/');

chrome.test.getConfig(async function(config) {
  const platformInfo = await chrome.runtime.getPlatformInfo();
  const isAndroid = platformInfo.os === 'android';
  const APP_NAME = isAndroid ? 'org.chromium.chrome.tests.support' :
                               'com.google.chrome.test.echo';

  function getHostNotFoundError(notFoundApp) {
    return isAndroid ? `Unable to connect to ${notFoundApp}.` :
                       'Specified native messaging host not found.';
  }

  // Tests calling with an app name that is not installed.
  function invalidHostNameCallback() {
    const invalidHostName = 'not.installed.app';
    const message = {text: 'Hello!'};
    chrome.runtime.sendNativeMessage(
        invalidHostName, message,
        chrome.test.callbackFail(
            getHostNotFoundError(invalidHostName), function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  }
  async function invalidHostNamePromise() {
    const invalidHostName = 'not.installed.app';
    const message = {test: 'Hello there!'};
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendNativeMessage(invalidHostName, message),
        `Error: ${getHostNotFoundError(invalidHostName)}`);
    chrome.test.succeed();
  }

  // Desktop: Tests calling with an app name that has a manifest, but no binary
  // behind it. See native_messaging_test_util for for information. Android:
  // Tests calling with an installed external app that cannot receive extension
  // messages.
  function nonexistentHostCallback() {
    const nonExistentHostname = 'com.google.chrome.test.host_binary_missing';
    const message = {text: 'Hello!'};
    chrome.runtime.sendNativeMessage(
        nonExistentHostname, message,
        chrome.test.callbackFail(
            getHostNotFoundError(nonExistentHostname), function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  }
  async function nonexistentHostPromise() {
    const nonExistentHostname = 'com.google.chrome.test.host_binary_missing';
    const message = {text: 'Hello!'};
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendNativeMessage(nonExistentHostname, message),
        `Error: ${getHostNotFoundError(nonExistentHostname)}`);
    chrome.test.succeed();
  }

  // Tests a successful call to an app that does exist and responds with an
  // echo. See native_messaging_test_util for for information.
  function sendMessageWithCallback() {
    const message = {text: 'Hi there!', number: 3};
    chrome.runtime.sendNativeMessage(
        APP_NAME, message, chrome.test.callbackPass(function(response) {
          chrome.test.assertEq(1, response.id);
          chrome.test.assertEq(message, response.echo);
          chrome.test.assertEq(extensionUrl, response.caller_url);
        }));
  }
  async function sendMessageWithPromise() {
    const message = {text: 'Hi there!', number: 3};
    const response = await chrome.runtime.sendNativeMessage(APP_NAME, message);
    chrome.test.assertEq(1, response.id);
    chrome.test.assertEq(message, response.echo);
    chrome.test.assertEq(extensionUrl, response.caller_url);
    chrome.test.succeed();
  }

  // Creates a special message for which the test host must try sending a
  // message that is bigger than the limit.
  function bigMessageCallback() {
    const message = {bigMessageTest: true};
    chrome.runtime.sendNativeMessage(
        APP_NAME, message,
        chrome.test.callbackFail(
            'Error when communicating with the native messaging host.',
            function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  }
  async function bigMessagePromise() {
    const message = {bigMessageTest: true};
    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendNativeMessage(APP_NAME, message),
        'Error: Error when communicating with the native messaging host.');
    chrome.test.succeed();
  }

  if (isAndroid) {
    // TODO(crbug.com/515159909): Add bigMessage tests to the list of tests
    // after larger messages are supported for Android.
    chrome.test.runTests([
      invalidHostNameCallback,
      invalidHostNamePromise,
      nonexistentHostCallback,
      nonexistentHostPromise,
      sendMessageWithCallback,
      sendMessageWithPromise,
    ]);
    return;
  }

  chrome.test.runTests([
    invalidHostNameCallback,
    invalidHostNamePromise,
    nonexistentHostCallback,
    nonexistentHostPromise,
    sendMessageWithCallback,
    sendMessageWithPromise,
    bigMessageCallback,
    bigMessagePromise,
  ]);
});
