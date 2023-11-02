// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

// A test tab which has the listener we are sending messages to injected as a
// content script.
var testTab = null;

chrome.test.runTests([
  async function setup() {
    const config = await chrome.test.getConfig();
    let url = `http://example.com:${config.testServer.port}/empty.html`;
    testTab = await openTab(url);
    chrome.test.succeed();
  },

  function sendMessageWithCallback() {
    chrome.tabs.sendMessage(testTab.id, 'ping', (response) => {
      chrome.test.assertNoLastError();
      chrome.test.assertEq('pong', response);
      chrome.test.succeed();
    });
  },

  async function sendMessageWithPromise() {
    const response = await chrome.tabs.sendMessage(testTab.id, 'ping');
    chrome.test.assertEq('pong', response);
    chrome.test.succeed();
  }
]);
