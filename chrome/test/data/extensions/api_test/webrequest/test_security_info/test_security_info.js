// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

(async () => {
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);
  const request_url = args.request_url;
  const certificate_bytes = args.certificate_bytes;
  const certificate_sha256 = args.certificate_sha256;
  const expect_state = args.expect_state;
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
        await fetch(request_url);
      }
    } catch (e) {
      chrome.test.fail(`Fetch failed: ${e.message}`);
    }
    listeners.forEach(listener => {
      chrome.webRequest.onHeadersReceived.removeListener(listener);
    });
  }

  runTests([
    function testSecurityInfoAbsentWithoutFlag() {
      var listener = callbackPass(function(details) {
        chrome.test.assertFalse('securityInfo' in details);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']}, ['extraHeaders']);

      makeRequest(listener);
    },

    function testSecurityInfoBasic() {
      var listener = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);

        chrome.test.assertEq(expect_state, details.securityInfo.state);
        chrome.test.assertEq(1, details.securityInfo.certificates.length);
        chrome.test.assertFalse(
            'rawDER' in details.securityInfo.certificates[0]);
        chrome.test.assertEq(
            certificate_sha256,
            details.securityInfo.certificates[0].fingerprint.sha256);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']}, ['securityInfo']);

      makeRequest(listener);
    },

    // Using only securityInfoRawDer dictionary member is the same as
    // using two dictionary members: securityInfo, securityInfoRawDer.
    function testSecurityInfoRawDer() {
      var listener = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq(1, details.securityInfo.certificates.length);

        let server_cert = details.securityInfo.certificates[0];
        chrome.test.assertTrue(server_cert.rawDER.byteLength > 0);
        chrome.test.assertEq(
            certificate_bytes, getPemEncodedFromDer(server_cert.rawDER));

        chrome.test.assertEq(
            certificate_sha256, server_cert.fingerprint.sha256);
        chrome.test.assertEq(expect_state, details.securityInfo.state);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']}, ['securityInfoRawDer']);

      makeRequest(listener);
    },

    function testSecurityInfoBothFlags() {
      var listener = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq(1, details.securityInfo.certificates.length);

        let server_cert = details.securityInfo.certificates[0];
        chrome.test.assertTrue(server_cert.rawDER.byteLength > 0);
        chrome.test.assertEq(
            certificate_bytes, getPemEncodedFromDer(server_cert.rawDER));

        chrome.test.assertEq(
            certificate_sha256, server_cert.fingerprint.sha256);
        chrome.test.assertEq(expect_state, details.securityInfo.state);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: ['<all_urls>']},
          ['securityInfo', 'securityInfoRawDer']);

      makeRequest(listener);
    },

    // Test that registered listener without securityInfo dictionary member of
    // ExtraInfoSpec will not receive SecurityInfo object even when there's a
    // second listener with securityInfo.
    function testOnlyOneListenerReceivesSecurityInfo() {
      var listener1 = callbackPass(function(details) {
        chrome.test.assertFalse('securityInfo' in details);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener1, {urls: ['<all_urls>']}, ['extraHeaders']);

      var listener2 = callbackPass(function(details) {
        chrome.test.assertTrue('securityInfo' in details);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener2, {urls: ['<all_urls>']}, ['securityInfo']);

      makeRequest(listener1, listener2);
    },

    function testOnlyOneListenerReceivesSecurityInfoRawDer() {
      var listener1 = callbackPass(function(details) {
        chrome.test.assertFalse(
            'rawDER' in details.securityInfo.certificates[0]);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener1, {urls: ['<all_urls>']}, ['securityInfo']);

      var listener2 = callbackPass(function(details) {
        chrome.test.assertTrue(
            'rawDER' in details.securityInfo.certificates[0]);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener2, {urls: ['<all_urls>']}, ['securityInfoRawDer']);

      makeRequest(listener1, listener2);
    },
  ]);
})();
