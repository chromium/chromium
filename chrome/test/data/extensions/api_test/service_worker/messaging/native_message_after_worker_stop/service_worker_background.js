// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const appName = 'com.google.chrome.test.echo';
const EXTENSION_URL = 'chrome-extension://knldjmfmopnpolahpmmgbagdohdnhkik/';

const sentMessage = {
  text: 'test-echo',
};
port = chrome.runtime.connectNative(appName);
port.onMessage.addListener((message) => {
  chrome.test.assertEq(sentMessage, message.echo);
  chrome.test.assertEq(EXTENSION_URL, message.caller_url);
  chrome.test.succeed();
});
port.postMessage(sentMessage);
