// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function replaceStarWithNoneListener(directive) {
  return function(details) {
    for (i = 0; i < details.responseHeaders.length; i++) {
      if (details.responseHeaders[i].name.toLowerCase() ===
          'content-security-policy') {
        chrome.test.assertEq(
            directive + ' *', details.responseHeaders[i].value);
        details.responseHeaders[i].value = directive + ' \'none\'';
      }
    }
    return {responseHeaders: details.responseHeaders};
  };
}

function navigate(url) {
  chrome.test.sendMessage(JSON.stringify({navigate: {tabId: tabId, url: url}}));
  return new Promise(resolve => {
    let listener = (_, info, tab) => {
      if (tab.id == tabId && info.status == 'complete') {
        chrome.tabs.onUpdated.removeListener(listener);
        resolve();
      }
    };
    chrome.tabs.onUpdated.addListener(listener);
  });
};

function resultFromTab(code) {
  return new Promise(
      resolve => chrome.tabs.executeScript(tabId, {code: code}, results => {
        chrome.test.assertEq(1, results.length);
        resolve(results[0]);
      }));
}


let checkIfIframeBlocked = `
  (function() {
    try {
      document.getElementById('iframe').contentWindow.location.href;
      return false;
    } catch {
      return true;
    }
  }());
`;

// Check of CSP directives in headers can be modified by extensions.
//
const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  // Test that modifications to CSP 'frame-ancestors' are honored.
  async function testModifyCSPHeaderFrameAncestors() {
    let url = getServerURL(
        'extensions/api_test/webrequest/csp/document-with-iframe.html');
    let headersListener = replaceStarWithNoneListener('frame-ancestors');
    chrome.webRequest.onHeadersReceived.addListener(
        headersListener, {
          urls: [getServerURL(
              'set-header?Content-Security-Policy%3A%20frame-ancestors%20*')]
        },
        ['blocking', 'responseHeaders']);

    await navigate(url);
    let blocked = await resultFromTab(checkIfIframeBlocked);
    chrome.test.assertTrue(blocked, 'CSP was not modified.');
    chrome.webRequest.onHeadersReceived.removeListener(headersListener);
    chrome.test.succeed();
  },

  // Test that modifications to CSP 'frame-ancestors' with the 'extraHeaders'
  // option are honored.
  async function testModifyCSPHeaderFrameAncestorsExtraHeaders() {
    let url = getServerURL(
        'extensions/api_test/webrequest/csp/document-with-iframe.html');
    let headersListener = replaceStarWithNoneListener('frame-ancestors');
    chrome.webRequest.onHeadersReceived.addListener(
        headersListener, {
          urls: [getServerURL(
              'set-header?Content-Security-Policy%3A%20frame-ancestors%20*')]
        },
        ['blocking', 'responseHeaders', 'extraHeaders']);

    await navigate(url);
    let blocked = await resultFromTab(checkIfIframeBlocked);
    chrome.test.assertTrue(blocked, 'CSP was not modified.');
    chrome.webRequest.onHeadersReceived.removeListener(headersListener);
    chrome.test.succeed();
  },

  // Test that modifications to CSP 'img-src' are honored.
  async function testModifyCSPHeaders() {
    let url = getServerURL('extensions/api_test/webrequest/csp/img.html');
    let headersListener = replaceStarWithNoneListener('img-src');
    chrome.webRequest.onHeadersReceived.addListener(
        headersListener, {urls: [url]}, ['blocking', 'responseHeaders']);

    await navigate(url);
    let blocked = await resultFromTab(
        'document.getElementById(\'result\').innerText === \'blocked\';');
    chrome.test.assertTrue(blocked, 'CSP was not modified.');
    chrome.webRequest.onHeadersReceived.removeListener(headersListener);
    chrome.test.succeed();
  },
])});
