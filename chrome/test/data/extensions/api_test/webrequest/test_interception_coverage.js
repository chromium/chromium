// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This script runs in a normal extension environment and verifies
 * that requests from an <iframe> are intercepted by the chrome.webRequest API.
 * It loads request_interception_coverage.js and calls run_tests().
 */

runTests([async function testRequestInterceptionCoverage() {
  const baseUrl = getServerURL('', '127.0.0.1');

  await new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = baseUrl + 'webview/request_interception_coverage.js';
    script.onload = resolve;
    script.onerror = reject;
    document.head.appendChild(script);
  });

  const expectedFailures = [
    {title: 'WebSocket in Shared Worker', event: 'onBeforeRequest'},
    {title: 'WebSocket in Service Worker', event: 'onBeforeRequest'},
  ];

  const result = await run_tests(
      'iframe', baseUrl, testWebSocketPort, testWebTransportPort,
      expectedFailures.map(f => f.title));
  const kExpectedResult =
      expectedFailures.map(f => `${f.title}: not observed by ${f.event}`)
          .join('\n');

  chrome.test.assertEq(kExpectedResult, result);
  chrome.test.succeed();
}]);
