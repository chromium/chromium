// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as util from '/_test_resources/api_test/web_authentication_proxy/util.js';

let availableTests = [
  async function incognitoAndRegular() {
    chrome.webAuthenticationProxy.onCreateRequest.addListener(
        async (request) => {
          await util.completeCreateRequest(request.requestId);
        });
    await chrome.webAuthenticationProxy.attach();
    if (chrome.extension.inIncognitoContext) {
      chrome.test.sendMessage('incognito ready');
    } else {
      chrome.test.sendMessage('regular ready');
    }
  },
  async function regularOnly() {
    if (chrome.extension.inIncognitoContext) {
      chrome.test.sendMessage('incognito ready');
    } else {
      chrome.webAuthenticationProxy.onCreateRequest.addListener(
          async (request) => {
            await util.completeCreateRequest(request.requestId);
          });
      await chrome.webAuthenticationProxy.attach();
      chrome.test.sendMessage('regular ready');
    }
  },
  async function incognitoOnly() {
    if (chrome.extension.inIncognitoContext) {
      chrome.webAuthenticationProxy.onCreateRequest.addListener(
          async (request) => {
            await util.completeCreateRequest(request.requestId);
          });
      await chrome.webAuthenticationProxy.attach();
      chrome.test.sendMessage('incognito ready');
    } else {
      chrome.test.sendMessage('regular ready');
    }
  },
];

chrome.test.getConfig((config) => {
  const tests = availableTests.filter((t) => {
    return config.customArg == t.name;
  });
  if (tests.length == 0) {
    // Log because the C++ side might stall rather than notice the call to
    // notifyFail.
    console.error('No test found');
    chrome.test.notifyFail('No test found');
    return;
  }
  chrome.test.runTests(tests);
});
