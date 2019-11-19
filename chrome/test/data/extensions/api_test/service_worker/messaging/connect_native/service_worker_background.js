// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const appName = 'com.google.chrome.test.echo';
const kExtensionURL  = 'chrome-extension://knldjmfmopnpolahpmmgbagdohdnhkik/';

// NOTE: These tests are based on (copied from =P)
// chrome/test/data/extensions/api_test/native_messaging/test.js.
// TODO(lazyboy): We should run tests with and without Service Worker based
// background from the same place! This will require some tweaking.
chrome.test.runTests([
  function connect() {
    var messagesToSend = [{text: 'foo'},
                          {text: 'bar', funCount: 9001},
                          {}];
    var currentMessage = 0;

    port = chrome.runtime.connectNative(appName);
    port.postMessage(messagesToSend[currentMessage]);

    port.onMessage.addListener(function(message) {
      chrome.test.assertEq(currentMessage + 1, message.id);
      chrome.test.assertEq(messagesToSend[currentMessage], message.echo);
      // NOTE: the original test [1] was using window.location.origin that
      // does not work in Service Workers, use kExtensionURL instead.
      chrome.test.assertEq(kExtensionURL, message.caller_url);
      currentMessage++;

      if (currentMessage == messagesToSend.length)
        chrome.test.succeed();
      else
        port.postMessage(messagesToSend[currentMessage]);
    });
  },

  // Verify that the case when host stops itself is handled properly.
  function stopHost() {
    port = chrome.runtime.connectNative(appName);

    port.onDisconnect.addListener(
        chrome.test.callback(function() {}, 'Native host has exited.'));

    // Send first message that should stop the host.
    port.postMessage({ 'stopHostTest': true });
  },
]);
