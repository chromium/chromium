// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Most of this file is copy-pasted from /api_test/native_messaging/lazy/test.js
// and then adapted to use MV3 and test promises.

var appName = 'com.google.chrome.test.echo';
var extensionUrl = chrome.runtime.getURL('/');
var hostNotFoundError = 'Specified native messaging host not found.';

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    // Tests calling with an app name that is not installed.
    function invalidHostNameCallback() {
      var message = {text: 'Hello!'};
      chrome.runtime.sendNativeMessage(
          'not.installed.app', message,
          chrome.test.callbackFail(hostNotFoundError, function(response) {
            chrome.test.assertEq(undefined, response);
          }));
    },
    async function invalidHostNamePromise() {
      var message = {test: 'Hello there!'};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage('not.installed.app', message),
          'Error: ' + hostNotFoundError);
      chrome.test.succeed();
    },

    // Tests calling with an app name that has a manifest, but no binary behind
    // it. See native_messaging_test_util for for information.
    function nonexistentHostCallback() {
      var message = {text: 'Hello!'};
      chrome.runtime.sendNativeMessage(
          'com.google.chrome.test.host_binary_missing', message,
          chrome.test.callbackFail(hostNotFoundError, function(response) {
            chrome.test.assertEq(undefined, response);
          }));
    },
    async function nonexistentHostPromise() {
      var message = {text: 'Hello!'};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage(
              'com.google.chrome.test.host_binary_missing', message),
          'Error: ' + hostNotFoundError);
      chrome.test.succeed();
    },

    // Tests a successful call to an app that does exist and responds with an
    // echo. See native_messaging_test_util for for information.
    function sendMessageWithCallback() {
      var message = {text: 'Hi there!', number: 3};
      chrome.runtime.sendNativeMessage(
          appName, message, chrome.test.callbackPass(function(response) {
            chrome.test.assertEq(1, response.id);
            chrome.test.assertEq(message, response.echo);
            chrome.test.assertEq(extensionUrl, response.caller_url);
          }));
    },
    async function sendMessageWithPromise() {
      var message = {text: 'Hi there!', number: 3};
      const response = await chrome.runtime.sendNativeMessage(appName, message);
      chrome.test.assertEq(1, response.id);
      chrome.test.assertEq(message, response.echo);
      chrome.test.assertEq(extensionUrl, response.caller_url);
      chrome.test.succeed();
    },

    // Creates a special message for which the test host must try sending a
    // message that is bigger than the limit.
    function bigMessageCallback() {
      var message = {bigMessageTest: true};
      chrome.runtime.sendNativeMessage(
          appName, message,
          chrome.test.callbackFail(
              'Error when communicating with the native messaging host.',
              function(response) {
                chrome.test.assertEq(undefined, response);
              }));
    },
    async function bigMessagePromise() {
      var message = {bigMessageTest: true};
      await chrome.test.assertPromiseRejects(
          chrome.runtime.sendNativeMessage(appName, message),
          'Error: Error when communicating with the native messaging host.');
      chrome.test.succeed();
    }
  ]);
});
