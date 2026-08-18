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

  chrome.test.runTests([
    function connect() {
      const messagesToSend = [{text: 'foo'}, {text: 'bar', funCount: 9001}, {}];
      let currentMessage = 0;

      const port = chrome.runtime.connectNative(APP_NAME);
      port.postMessage(messagesToSend[currentMessage]);

      port.onMessage.addListener(function(message) {
        chrome.test.assertEq(currentMessage + 1, message.id);
        chrome.test.assertEq(messagesToSend[currentMessage], message.echo);
        checkMessageUrl(message.caller_url);
        currentMessage++;

        if (currentMessage === messagesToSend.length) {
          chrome.test.succeed();
        } else {
          port.postMessage(messagesToSend[currentMessage]);
        }
      });
    },

    // Verify that the case when host stops itself is handled properly.
    function stopHost() {
      const port = chrome.runtime.connectNative(APP_NAME);

      // On Android, the app can close the port by itself without invoking an
      // error.
      port.onDisconnect.addListener(
          isAndroid ?
              chrome.test.callbackPass() :
              chrome.test.callback(function() {}, 'Native host has exited.'));

      // Send first message that should stop the host.
      port.postMessage({stopHostTest: true});
    },

    // The following tests check that multiple ports are isolated.
    // - On desktop, each connectNative call spawns a separate native host
    //   process so the ports are isolated.
    // - On Android, test that ports are still isolated even though one process
    //   from the external app handles all communications from the browser.
    //   Note that the test Android app (EchoService.java) also isolates ports
    //   from connectNative calls and a poorly implemented app can break these
    //   assumptions.

    // Test that messages exchanged in multiple connected ports are isolated
    // from one another.
    function connectMultiplePorts() {
      const messages1 = [
        {port: 1, text: 'alpha'},
        {port: 1, text: 'beta', seq: 2},
      ];
      const messages2 = [
        {port: 2, text: 'gamma'},
        {port: 2, text: 'delta', seq: 2},
        {port: 2, text: 'epsilon', seq: 3},
      ];

      let count1 = 0;
      let count2 = 0;

      // If both counters above match the number of messages sent, pass the
      // test. This means all messages have been sent and echoed back.
      function checkCompletion() {
        if (count1 === messages1.length && count2 === messages2.length) {
          port1.disconnect();
          port2.disconnect();
          chrome.test.succeed();
        }
      }

      // Two ports running identical listeners:
      // - Asserts that a sent message is echoed back.
      // - Increments a counter for each echoed message.
      // - Checks if all messages between both ports have been echoed.
      const port1 = chrome.runtime.connectNative(APP_NAME);
      port1.onMessage.addListener(function(message) {
        chrome.test.assertEq(messages1[count1], message.echo);
        count1++;
        checkCompletion();
      });

      const port2 = chrome.runtime.connectNative(APP_NAME);
      port2.onMessage.addListener(function(message) {
        chrome.test.assertEq(messages2[count2], message.echo);
        count2++;
        checkCompletion();
      });

      for (const msg of messages1) {
        port1.postMessage(msg);
      }
      for (const msg of messages2) {
        port2.postMessage(msg);
      }
    },

    // Test that disconnecting one port from the extension does not affect other
    // connected ports.
    function disconnectOnePortByBrowser() {
      const port1 = chrome.runtime.connectNative(APP_NAME);
      const port2 = chrome.runtime.connectNative(APP_NAME);

      port1.onMessage.addListener(function(message) {
        chrome.test.assertEq({msg: 1, port: 1}, message.echo);
        port1.disconnect();
        chrome.test.succeed();
      });

      port2.onMessage.addListener(function(message) {
        chrome.test.assertEq({msg: 1, port: 2}, message.echo);
        // Disconnect `port2` after receiving its reply.
        port2.disconnect();

        // Now post a message to `port1`. This should trigger its listener which
        // will end the test.
        port1.postMessage({msg: 1, port: 1});
      });

      port2.postMessage({msg: 1, port: 2});
    },

    // Test that disconnecting one port from the app does not affect other
    // connected ports.
    function disconnectOnePortByApp() {
      const port1 = chrome.runtime.connectNative(APP_NAME);
      const port2 = chrome.runtime.connectNative(APP_NAME);

      port1.onMessage.addListener(function(message) {
        chrome.test.assertEq({msg: 1, port: 1}, message.echo);
        port1.disconnect();
        chrome.test.succeed();
      });

      port2.onDisconnect.addListener(function() {
        if (isAndroid) {
          chrome.test.assertNoLastError();
        } else {
          chrome.test.assertLastError('Native host has exited.');
        }

        // Now post a message to `port1`. This should trigger its listener which
        // will end the test.
        port1.postMessage({msg: 1, port: 1});
      });

      // Send message to stop `port2` from the app side.
      port2.postMessage({stopHostTest: true});
    },
  ]);
});
