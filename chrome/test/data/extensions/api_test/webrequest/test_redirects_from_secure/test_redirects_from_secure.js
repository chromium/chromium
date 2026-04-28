// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function assertRedirectSucceeds(url, redirectURL, callback) {
  navigateAndWait(url, function(tab) {
    if (callback) {
      callback();
    }
    chrome.test.assertEq(redirectURL, tab.url);
  });
}

const SCRIPT_URL = '_test_resources/api_test/webrequest/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.getConfig((config) => {
    const customArg = JSON.parse(config.customArg);
    const startingURL = customArg[0];
    const redirectURL = customArg[1];

    runTests([
      function redirectToInsecure() {
        const listener = function(details) {
          if (details.url.endsWith('page_with_referrer.html')) {
            return {redirectUrl: redirectURL};
          }
        };
        chrome.webRequest.onHeadersReceived.addListener(
            listener, {urls: ['<all_urls>']}, ['blocking']);

        const errorListener = function(details) {
          chrome.test.fail();
        };
        chrome.webRequest.onErrorOccurred.addListener(
            errorListener, {urls: [redirectURL]});

        assertRedirectSucceeds(startingURL, redirectURL, function() {
          chrome.webRequest.onHeadersReceived.removeListener(listener);
        });
      },
    ]);
  });
});
