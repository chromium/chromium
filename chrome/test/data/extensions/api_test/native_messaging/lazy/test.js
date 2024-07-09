// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appName = 'com.google.chrome.test.echo';
var inServiceWorker = 'ServiceWorkerGlobalScope' in self;
var extensionUrl = chrome.runtime.getURL('/');

function checkMessageUrl(url) {
  if (inServiceWorker) {
    chrome.test.assertEq(extensionUrl, url);
  } else {
    chrome.test.assertEq(window.location.origin + '/', url);
  }
}

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function invalidHostName() {
      var message = {'text': 'Hello!'};
      chrome.runtime.sendNativeMessage(
          'not.installed.app', message,
          chrome.test.callbackFail(
              'Specified native messaging host not found.', function(response) {
                chrome.test.assertEq(undefined, response);
              }));
    },

    function nonexistentHost() {
      var message = {'text': 'Hello!'};
      chrome.runtime.sendNativeMessage(
          'com.google.chrome.test.host_binary_missing', message,
          chrome.test.callbackFail(
              'Specified native messaging host not found.', function(response) {
                chrome.test.assertEq(undefined, response);
              }));
    },

    function sendMessageWithCallback() {
      var message = {'text': 'Hi there!', 'number': 3};
      chrome.runtime.sendNativeMessage(
          appName, message, chrome.test.callbackPass(function(response) {
            chrome.test.assertEq(1, response.id);
            chrome.test.assertEq(message, response.echo);
            checkMessageUrl(response.caller_url);
          }));
    },

    // The goal of this test is just not to crash.
    function sendMessageWithoutCallback() {
      var message = {'text': 'Hi there!', 'number': 3};
      chrome.runtime.sendNativeMessage(appName, message);
      chrome.test.succeed();  // Mission Complete
    },

    function bigMessage() {
      // Create a special message for which the test host must try sending a
      // message that is bigger than the limit.
      var message = {'bigMessageTest': true};
      chrome.runtime.sendNativeMessage(
          appName, message,
          chrome.test.callbackFail(
              'Error when communicating with the native messaging host.',
              function(response) {
                chrome.test.assertEq(undefined, response);
              }));
    },

    function invalidMessage() {
      // Create a special message that's not valid JSON.
      var message = {sendInvalidResponse: true};
      chrome.runtime.sendNativeMessage(
          appName, message,
          chrome.test.callbackFail(
              'The sender sent an invalid JSON message; message ignored.',
              function(response) {
                chrome.test.assertEq(undefined, response);
              }));
    }
  ]);
});
