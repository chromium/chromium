// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

(async () => {
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);
  const requestUrl = args.request_url;
  const useWebSocket = Boolean(args.use_web_socket);

  // The filter is used to prevent flakiness which results in
  // catching web requests that we are not interested with.
  let filter;
  if (useWebSocket) {
    const urlObj = new URL(requestUrl);
    filter = [
      // Scheme wildcard (*) does not include ws(s).
      `*://${urlObj.hostname}${urlObj.pathname}`,
      `ws://${urlObj.hostname}${urlObj.pathname}`,
      `wss://${urlObj.hostname}${urlObj.pathname}`,
    ];
  } else {
    filter = [requestUrl];
  }

  const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
  await chrome.test.loadScript(scriptUrl);

  async function openWebSocket() {
    return new Promise((resolve) => {
      const socket = new WebSocket(requestUrl);

      socket.onopen = (_event) => {
        socket.close();
        resolve();
      };
    });
  }

  async function makeRequest() {
    const requestDone = callbackPass(() => {});
    await new Promise(
        resolve => chrome.webRequest.handlerBehaviorChanged(resolve));
    try {
      if (useWebSocket) {
        await openWebSocket();
      } else {
        const response = await fetch(requestUrl);
        await response.text();
      }
      requestDone();
    } catch (e) {
      chrome.test.fail(`Fetch failed: ${e.message}`);
    }
  }

  function verifyInsecureSecurityInfo(details) {
    chrome.test.assertTrue('securityInfo' in details);
    chrome.test.assertEq('insecure', details.securityInfo.state);
    chrome.test.assertFalse('certificates' in details.securityInfo);
    chrome.test.assertFalse('protocolVersion' in details.securityInfo);
    chrome.test.assertFalse('cipherSuite' in details.securityInfo);
  }

  runTests([
    function testSecurityInfoFlagInsecure() {
      const onResponseStartedListener = callbackPass(function(details) {
        chrome.webRequest.onResponseStarted.removeListener(
            onResponseStartedListener);
        chrome.test.assertEq('127.0.0.1', details.ip);
      });
      chrome.webRequest.onResponseStarted.addListener(
          onResponseStartedListener, {urls: filter});

      const listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
        verifyInsecureSecurityInfo(details);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfo']);

      makeRequest();
    },

    function testSecurityInfoRawDerFlagInsecure() {
      const listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
        verifyInsecureSecurityInfo(details);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfoRawDer']);

      makeRequest();
    },

    function testSecurityInfoBothFlagsInsecure() {
      const listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
        verifyInsecureSecurityInfo(details);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfo', 'securityInfoRawDer']);

      makeRequest();
    },

    function testSecurityInfoOnCompletedInsecure() {
      const listener = callbackPass(function(details) {
        chrome.webRequest.onCompleted.removeListener(listener);
        chrome.test.assertEq('127.0.0.1', details.ip);
        verifyInsecureSecurityInfo(details);
      });

      chrome.webRequest.onCompleted.addListener(
          listener, {urls: filter}, ['securityInfo']);

      makeRequest();
    },
  ]);
})();
