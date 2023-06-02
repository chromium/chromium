// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appName = 'com.google.chrome.test.exe.echo';

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function sendMessageWithCallback() {
      var message = {echo: 'TryCallback', id: 1};
      chrome.runtime.sendNativeMessage(
          appName, message, chrome.test.callbackPass(function(response) {
            chrome.test.assertEq(1, response.id);
            chrome.test.assertEq('TryCallback', response.echo);
          }));
    },
    async function sendMessageWithPromise() {
      var message = {echo: 'TryPromise', id: 2};
      const response = await chrome.runtime.sendNativeMessage(appName, message);
      chrome.test.assertEq(2, response.id);
      chrome.test.assertEq('TryPromise', response.echo);
      chrome.test.succeed();
    },
  ]);
});