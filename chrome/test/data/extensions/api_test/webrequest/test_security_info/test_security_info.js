// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

// Checks that a string is formatted properly as sha256 fingerprint
// which is formatted as 31 pairs of "XX:" followed by one "XX".
function isSha256Fingerprint(input) {
  const fingerprintRegex = /^([0-9A-F]{2}:){31}[0-9A-F]{2}$/;
  return fingerprintRegex.test(input);
}

(async () => {
  const config = await new Promise(resolve => chrome.test.getConfig(resolve));
  const args = JSON.parse(config.customArg);
  const request_url = args.request_url;
  const certificate_bytes = args.certificate_bytes;
  const expect_state = args.expect_state;
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
    function testSecurityInfoAbsentWithoutFlag() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);
        chrome.test.assertFalse('securityInfo' in details);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['extraHeaders']);

      makeRequest();
    },

    function testSecurityInfoBasic() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);

        chrome.test.assertTrue('securityInfo' in details);

        chrome.test.assertEq(expect_state, details.securityInfo.state);
        chrome.test.assertEq(1, details.securityInfo.certificates.length);
        chrome.test.assertFalse(
            'rawDER' in details.securityInfo.certificates[0]);
        chrome.test.assertTrue(isSha256Fingerprint(
            details.securityInfo.certificates[0].fingerprint.sha256));
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfo']);

      makeRequest();
    },

    // Using only securityInfoRawDer dictionary member is the same as
    // using two dictionary members: securityInfo, securityInfoRawDer.
    function testSecurityInfoRawDer() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);

        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq(1, details.securityInfo.certificates.length);

        let server_cert = details.securityInfo.certificates[0];
        chrome.test.assertTrue(server_cert.rawDER.byteLength > 0);
        chrome.test.assertEq(
            certificate_bytes, getPemEncodedFromDer(server_cert.rawDER));


        chrome.test.assertTrue(isSha256Fingerprint(
            details.securityInfo.certificates[0].fingerprint.sha256));
        chrome.test.assertEq(expect_state, details.securityInfo.state);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfoRawDer']);

      makeRequest();
    },

    function testSecurityInfoBothFlags() {
      let listener;
      listener = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener);

        chrome.test.assertTrue('securityInfo' in details);
        chrome.test.assertEq(1, details.securityInfo.certificates.length);

        let server_cert = details.securityInfo.certificates[0];
        chrome.test.assertTrue(server_cert.rawDER.byteLength > 0);
        chrome.test.assertEq(
            certificate_bytes, getPemEncodedFromDer(server_cert.rawDER));


        chrome.test.assertTrue(isSha256Fingerprint(
            details.securityInfo.certificates[0].fingerprint.sha256));
        chrome.test.assertEq(expect_state, details.securityInfo.state);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener, {urls: filter}, ['securityInfo', 'securityInfoRawDer']);

      makeRequest();
    },

    // Test that registered listener without securityInfo dictionary member of
    // ExtraInfoSpec will not receive SecurityInfo object even when there's a
    // second listener with securityInfo.
    function testOnlyOneListenerReceivesSecurityInfo() {
      let listener1;
      listener1 = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener1);
        chrome.test.assertFalse('securityInfo' in details);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener1, {urls: filter}, ['extraHeaders']);

      let listener2;
      listener2 = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener2);
        chrome.test.assertTrue('securityInfo' in details);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener2, {urls: filter}, ['securityInfo']);

      makeRequest();
    },

    function testOnlyOneListenerReceivesSecurityInfoRawDer() {
      let listener1;
      listener1 = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener1);

        chrome.test.assertFalse(
            'rawDER' in details.securityInfo.certificates[0]);
      });
      chrome.webRequest.onHeadersReceived.addListener(
          listener1, {urls: filter}, ['securityInfo']);

      let listener2;
      listener2 = callbackPass(function(details) {
        chrome.webRequest.onHeadersReceived.removeListener(listener2);

        chrome.test.assertTrue(
            'rawDER' in details.securityInfo.certificates[0]);
      });

      chrome.webRequest.onHeadersReceived.addListener(
          listener2, {urls: filter}, ['securityInfoRawDer']);

      makeRequest();
    },
  ]);
})();
