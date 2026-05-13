// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function navigateTab(url) {
  return new Promise(resolve => {
    chrome.tabs.onUpdated.addListener(function updateCallback(_, info, tab) {
      if (info.status === 'complete' && tab.url === url) {
        chrome.tabs.onUpdated.removeListener(updateCallback);
        resolve(tab);
      }
    });

    chrome.tabs.update(null, {url: url, active: true});
  });
}

let testServerPort;
function getServerURL(path, host) {
  return `http://${host}:${testServerPort}/${path}`;
}

const headerRemovingListener = function(details) {
  const headers = details.requestHeaders;
  const userAgentIndex =
      headers.findIndex((header) => header.name.toLowerCase() === 'user-agent');
  headers.splice(userAgentIndex, 1);
  return {requestHeaders: headers};
};

const headerModifyingListener = function(details) {
  const headers = details.requestHeaders;
  const userAgent =
      headers.find((header) => header.name.toLowerCase() === 'user-agent');
  userAgent.value = 'bar';
  return {requestHeaders: headers};
};

function expectTabResponse(value, message) {
  return new Promise(resolve => {
           const messageListener = request => {
             chrome.runtime.onMessage.removeListener(messageListener);
             resolve(request);
           };
           chrome.runtime.onMessage.addListener(messageListener);
         })
      .then((result) => chrome.test.assertEq(value, result, message))
      .then(chrome.test.succeed);
}

chrome.test.getConfig(function(config) {
  testServerPort = config.testServer.port;

  const navigationURL = getServerURL('echoheader?cookie', 'xyz.com');
  const fetchCode = (host) => `(async function () {
    let text = await fetch("${getServerURL('echoheader?user-agent', host)}")
                    .then((rsp) => rsp.text());
    chrome.runtime.sendMessage(text);
  })()`;

  chrome.test.runTests([
    async function testExtensionRemoveUserAgent() {
      // Test that the webRequest API may remove the User-Agent, but that
      // the request still goes ahead with no preflight.
      chrome.webRequest.onBeforeSendHeaders.addListener(
          headerRemovingListener, {urls: ['*://remove.cors.com/*']},
          ['requestHeaders', 'extraHeaders', 'blocking']);

      navigateTab(navigationURL).then((tab) => {
        chrome.tabs.executeScript(
            tab.id, {code: fetchCode('remove.cors.com')},
            () => expectTabResponse('None', 'User-Agent should be removed.'));
      });
    },
    async function testExtensionModifyUserAgent() {
      // Test that the webRequest API may set a User-Agent without
      // triggering a preflight.
      chrome.webRequest.onBeforeSendHeaders.addListener(
          headerModifyingListener, {urls: ['*://modify.cors.com/*']},
          ['requestHeaders', 'extraHeaders', 'blocking']);
      // 'extraHeaders' is required to modify User-Agent.

      navigateTab(navigationURL).then((tab) => {
        chrome.tabs.executeScript(
            tab.id, {code: fetchCode('modify.cors.com')},
            () => expectTabResponse('bar', 'User-Agent should be modified.'));
      });
    },
  ]);
});
