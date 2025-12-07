// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

(async () => {
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);
  const request_url = args.request_url;
  const use_web_socket = Boolean(args.use_web_socket);

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

  async function makeRequest(...listeners) {
    try {
      if (use_web_socket) {
        await openWebSocket();
      } else {
        await fetch(
            getServerURL(opt_path = 'simple.html', opt_scheme = 'https'));
      }
    } catch (e) {
      chrome.test.fail(`Fetch failed: ${e.message}`);
    }
    listeners.forEach(listener => {
      chrome.webRequest.onHeadersReceived.removeListener(listener);
    });
  }

  runTests([
    function testSecurityInfoFlagInsecure() {
      var listener = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq('insecure', details.securityInfo.state);
        chrome.test.assertFalse('certificates' in details.securityInfo);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']}, ['securityInfo']);

      makeRequest(listener);
    },

    function testSecurityInfoRawDerFlagInsecure() {
      var listener = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq('insecure', details.securityInfo.state);
        chrome.test.assertFalse('certificates' in details.securityInfo);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']}, ['securityInfoRawDer']);

      makeRequest(listener);
    },

    function testSecurityInfoBothFlagsInsecure() {
      var listener = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq('insecure', details.securityInfo.state);
        chrome.test.assertFalse('certificates' in details.securityInfo);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']},
          ['securityInfo', 'securityInfoRawDer']);

      makeRequest(listener);
    },
  ]);
})();
