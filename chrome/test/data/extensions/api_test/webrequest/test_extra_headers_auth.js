// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;

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
]);
