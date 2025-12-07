// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openTab} from '/_test_resources/test_util/tabs_util.js';

var testTab = null;

chrome.test.runTests([
  async function setup() {
    const config = await chrome.test.getConfig();
    let url = `http://example.com:${config.testServer.port}/empty.html`;
    testTab = await openTab(url);
    chrome.test.succeed();
  },

  async function sendMessageWithPromise() {
    const response = await chrome.tabs.sendMessage(testTab.id, 'ping');
    chrome.test.assertEq('pong', response.message);
    chrome.test.assertEq(
        chrome.runtime.getURL('options.html'), response.senderUrl);
    chrome.test.succeed();
  }
]);
