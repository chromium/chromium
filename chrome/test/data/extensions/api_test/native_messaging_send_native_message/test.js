// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this file is copy-pasted from /api_test/native_messaging/lazy/test.js
// and then adapted to use MV3 and test promises.

const APP_NAME = 'com.google.chrome.test.echo';
const extensionUrl = chrome.runtime.getURL('/');
const HOST_NOT_FOUND_ERROR = 'Specified native messaging host not found.';

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    // Tests calling with an app name that is not installed.
    function invalidHostNameCallback() {
      const message = {text: 'Hello!'};
      chrome.runtime.sendNativeMessage(
          'not.installed.app', message,
          chrome.test.callbackFail(HOST_NOT_FOUND_ERROR, function(response) {
            chrome.test.assertEq(undefined, response);
          }));
    },
    async function invalidHostNamePromise() {
      const message = {test: 'Hello there!'};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage('not.installed.app', message),
          'Error: ' + HOST_NOT_FOUND_ERROR);
      chrome.test.succeed();
    },

    // Tests calling with an app name that has a manifest, but no binary behind
    // it. See native_messaging_test_util for for information.
    function nonexistentHostCallback() {
      const message = {text: 'Hello!'};
      chrome.runtime.sendNativeMessage(
          'com.google.chrome.test.host_binary_missing', message,
          chrome.test.callbackFail(HOST_NOT_FOUND_ERROR, function(response) {
            chrome.test.assertEq(undefined, response);
          }));
    },
    async function nonexistentHostPromise() {
      const message = {text: 'Hello!'};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage(
              'com.google.chrome.test.host_binary_missing', message),
          'Error: ' + HOST_NOT_FOUND_ERROR);
      chrome.test.succeed();
    },

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
    },
    async function sendMessageWithPromise() {
      const message = {text: 'Hi there!', number: 3};
      const response =
          await chrome.runtime.sendNativeMessage(APP_NAME, message);
      chrome.test.assertEq(1, response.id);
      chrome.test.assertEq(message, response.echo);
      chrome.test.assertEq(extensionUrl, response.caller_url);
      chrome.test.succeed();
    },

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
    },
    async function bigMessagePromise() {
      const message = {bigMessageTest: true};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage(APP_NAME, message),
          'Error: Error when communicating with the native messaging host.');
      chrome.test.succeed();
    },
  ]);
});
