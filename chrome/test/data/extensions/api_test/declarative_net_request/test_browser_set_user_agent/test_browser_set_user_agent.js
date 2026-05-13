// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function doFetchEchoHeaders(host, port, doPreflight) {
  const modifiedHeaders = doPreflight ? {'User-Agent': 'foo'} : {};
  return fetch(
             `http://${host}:${port}/echoheader?User-Agent`,
             {headers: modifiedHeaders, cache: 'no-store'})
      .then((rsp) => rsp.text());
}

function navigateTab(url) {
  return new Promise(resolve => {
    chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
      if (info.status === 'complete' && tab.url === url) {
        chrome.tabs.onUpdated.removeListener(updateCallback);
        resolve(tab);
      }
    });

    chrome.tabs.update(null, {url, active: true});
  });
}

chrome.test.getConfig(function(config) {
  const testServerPort = config.testServer.port;

  chrome.test.runTests([
    async function testExtensionDNRRemoveUserAgent() {
      // Test that the DNR API may remove the User-Agent, and the request will
      // still succeed with no preflight.

      // Navigate to some arbitrary, working URL.
      navigateTab(`http://xyz.com:${testServerPort}/echoheader?cookie`)
          .then(
              // Do a fetch in-tab, cross-domain, to /echoheader?user-agent.
              (tab) => chrome.scripting.executeScript({
                target: {tabId: tab.id, allFrames: true},
                func: doFetchEchoHeaders,
                args:
                    ['remove.cors.com', testServerPort, /*doPreflight=*/ false],
              }))
          .then(
              // The response should be 'None'.
              (text) => chrome.test.assertEq(
                  'None', text[0].result, 'User-Agent should be removed.'))
          .then(chrome.test.succeed);
    },
    async function testExtensionDNRModifyUserAgent() {
      // Test that the DNR API may modify the User-Agent, and the request will
      // still succeed with no preflight.

      navigateTab(`http://xyz.com:${testServerPort}/echoheader?cookie`)
          .then((tab) => chrome.scripting.executeScript({
            target: {tabId: tab.id, allFrames: true},
            func: doFetchEchoHeaders,
            args: ['modify.cors.com', testServerPort, /*doPreflight=*/ false],
          }))
          .then(
              (text) => chrome.test.assertEq(
                  'bar', text[0].result, 'User-Agent should be modified.'))
          .then(chrome.test.succeed);
    },
    async function testCorsStillBlocksFetchUserAgent() {
      // Test that if DNR removes a User-Agent, the request still triggers a
      // preflight. This is the behaviour that all fetch-headers follow, and
      // User-Agent should not be special.

      navigateTab(`http://xyz.com:${testServerPort}/echoheader?cookie`)
          .then((tab) => chrome.scripting.executeScript({
            target: {tabId: tab.id, allFrames: true},
            func: doFetchEchoHeaders,
            args: ['remove.cors.com', testServerPort, /*doPreflight=*/ true],
          }))
          .then(
              (text) => chrome.test.assertEq(
                  null, text[0].result, 'Request should still fail.'))
          .then(chrome.test.succeed);
    },
  ]);
});
