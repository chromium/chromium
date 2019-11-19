// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appName = 'com.google.chrome.test.inbound_native_echo';

chrome.runtime.onConnectNative.addListener(port => {
  chrome.test.getConfig(function(config) {
    chrome.test.runTests([
      function sender() {
        chrome.test.assertEq(port.sender.nativeApplication, appName);
        chrome.test.succeed();
      },

      function sendMessages() {
        var messagesToSend =
            [{'text': 'foo'}, {'text': 'bar', 'funCount': 9001}, {}];
        var currentMessage = 0;

        port.postMessage(messagesToSend[currentMessage]);

        function onMessage(message) {
          chrome.test.assertEq(currentMessage + 1, message.id);
          chrome.test.assertEq(messagesToSend[currentMessage], message.echo);
          chrome.test.assertEq(
              message.caller_url, window.location.origin + '/');

          chrome.test.assertTrue(!!message.args);
          chrome.test.assertTrue(message.args.includes(
              '--native-messaging-connect-extension=' +
              document.location.host));
          chrome.test.assertTrue(message.args.includes(
              '--native-messaging-connect-host=' + appName));
          chrome.test.assertEq('test-connect-id', message.connect_id);

          currentMessage++;

          if (currentMessage == messagesToSend.length) {
            port.onMessage.removeListener(onMessage);
            chrome.test.succeed();
          } else {
            port.postMessage(messagesToSend[currentMessage]);
          }
        };
        port.onMessage.addListener(onMessage);
      },

      // Verify that the case when host stops itself is handled properly.
      function stopHost() {
        port.onDisconnect.addListener(
            chrome.test.callback(function() {}, 'Native host has exited.'));

        // Send first message that should stop the host.
        port.postMessage({'stopHostTest': true});
      }
    ]);
  });
});
