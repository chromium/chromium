// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

const SCRIPT_URL = '_test_resources/api_test/webrequest/framework.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  runTests([
    function testSpecialResponseHeadersVisibleForAuth() {
      const url = getServerURL('auth-basic?set-cookie-if-challenged');
      const extraHeadersListener = callbackPass(function(details) {
        checkHeaders(details.responseHeaders, ['set-cookie'], []);
      });
      chrome.webRequest.onAuthRequired.addListener(
          extraHeadersListener, {urls: [url]},
          ['responseHeaders', 'extraHeaders']);

      const standardListener = callbackPass(function(details) {
        checkHeaders(details.responseHeaders, [], ['set-cookie']);
      });
      chrome.webRequest.onAuthRequired.addListener(
          standardListener, {urls: [url]}, ['responseHeaders']);

      navigateAndWait(url, function() {
        chrome.webRequest.onAuthRequired.removeListener(extraHeadersListener);
        chrome.webRequest.onAuthRequired.removeListener(standardListener);
      });
    },
  ]);
});
