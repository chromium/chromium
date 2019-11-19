// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const appName = 'com.google.chrome.test.echo';
const kExtensionURL  = 'chrome-extension://knldjmfmopnpolahpmmgbagdohdnhkik/';

// NOTE: These tests are based on (copied from =P)
// chrome/test/data/extensions/api_test/native_messaging/test.js [1].
// TODO(lazyboy): We should run tests with and without Service Worker based
// background from the same place! This will require some tweaking.
chrome.test.runTests([
  function invalidHostName() {
    console.log('invalid host -name test');
    var message = {'text': 'Hello!'};
    chrome.runtime.sendNativeMessage(
        'not.installed.app', message,
        chrome.test.callbackFail(
            'Specified native messaging host not found.',
            function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  },

  function nonexistentHost() {
    var message = {text: 'Hello!'};
    chrome.runtime.sendNativeMessage(
        'com.google.chrome.test.host_binary_missing', message,
        chrome.test.callbackFail(
            'Specified native messaging host not found.',
            function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  },

  function sendMessageWithCallback() {
    var message = {text: 'Hi there!', number: 3};
    chrome.runtime.sendNativeMessage(
        appName, message,
        chrome.test.callbackPass(function(response) {
      chrome.test.assertEq(1, response.id);
      chrome.test.assertEq(message, response.echo);
      // NOTE: the original test [1] was using window.location.origin that
      // does not work in Service Workers, use kExtensionURL instead.
      chrome.test.assertEq(kExtensionURL, response.caller_url);
    }));
  },

  // The goal of this test is just not to crash.
  function sendMessageWithoutCallback() {
    var message = {text: 'Hi there!', number: 3};
    chrome.extension.sendNativeMessage(appName, message);
    chrome.test.succeed(); // Mission Complete
  },

  function bigMessage() {
    // Create a special message for which the test host must try sending a
    // message that is bigger than the limit.
    var message = {bigMessageTest: true};
    chrome.runtime.sendNativeMessage(
        appName, message,
        chrome.test.callbackFail(
            'Error when communicating with the native messaging host.',
            function(response) {
              chrome.test.assertEq(undefined, response);
            }));
  },
]);
