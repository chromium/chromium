// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

const scriptUrl = '_test_resources/api_test/webrequest/framework.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  runTests([
  function testSpecialResponseHeadersVisibleForAuth() {
    var url = getServerURL('auth-basic?set-cookie-if-challenged');
    var extraHeadersListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, ['set-cookie'], []);
    });
    chrome.webRequest.onAuthRequired.addListener(extraHeadersListener,
        {urls: [url]}, ['responseHeaders', 'extraHeaders']);

    var standardListener = callbackPass(function(details) {
      checkHeaders(details.responseHeaders, [], ['set-cookie']);
    });
    chrome.webRequest.onAuthRequired.addListener(standardListener,
        {urls: [url]}, ['responseHeaders']);

    navigateAndWait(url, function() {
      chrome.webRequest.onAuthRequired.removeListener(extraHeadersListener);
      chrome.webRequest.onAuthRequired.removeListener(standardListener);
    });
  }
])});
