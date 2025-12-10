// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

(async () => {
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);
  const request_url = args.request_url;
  const use_web_socket = Boolean(args.use_web_socket);

  // The filter is used to prevent flakiness which results in
  // catching web requests that we are not interested with.
  let filter;
  if (use_web_socket) {
    const urlObj = new URL(request_url);
    filter = [
      // Scheme wildcard (*) does not include ws(s).
      `*://${urlObj.hostname}${urlObj.pathname}`,
      `ws://${urlObj.hostname}${urlObj.pathname}`,
      `wss://${urlObj.hostname}${urlObj.pathname}`
    ];
  } else {
    filter = [request_url];
  }

  const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
  await chrome.test.loadScript(scriptUrl);

  async function openWebSocket() {
    return new Promise((resolve) => {
      const socket = new WebSocket(request_url);

      socket.onopen = (_event) => {
        socket.close();
        resolve();
      };
    });
  }

  async function makeRequest() {
    try {
      if (use_web_socket) {
        await openWebSocket();
      } else {
        await fetch(request_url);
      }
    } catch (e) {
      chrome.test.fail(`Fetch failed: ${e.message}`);
    }
  }

  runTests([
    function testSecurityInfoFlagInsecure() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);

        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq('insecure', details.securityInfo.state);
        chrome.test.assertFalse('certificates' in details.securityInfo);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfo']);

      makeRequest();
    },

    function testSecurityInfoRawDerFlagInsecure() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);

        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq('insecure', details.securityInfo.state);
        chrome.test.assertFalse('certificates' in details.securityInfo);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfoRawDer']);

      makeRequest();
    },

    function testSecurityInfoBothFlagsInsecure() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);

        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq('insecure', details.securityInfo.state);
        chrome.test.assertFalse('certificates' in details.securityInfo);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfo', 'securityInfoRawDer']);

      makeRequest();
    },
  ]);
})();
