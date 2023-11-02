// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tab where the content script has been injected.
var testTabId;

chrome.test.getConfig(function(config) {

  function rewriteURL(url) {
    return url.replace(/PORT/, config.testServer.port);
  }

  function doReq(domain, expectSuccess) {
    var url = rewriteURL(domain + ':PORT/extensions/test_file.txt');

    chrome.tabs.sendRequest(testTabId, url, function(response) {
      if (response.thrownError) {
        chrome.test.fail(response.thrownError);
        return;
      }
      if (expectSuccess) {
        chrome.test.assertEq('load', response.event);
        if (/^https?:/i.test(url))
          chrome.test.assertEq(200, response.status);
        chrome.test.assertEq('Hello!', response.text);
      } else {
        chrome.test.assertEq('error', response.event);
        chrome.test.assertEq(0, response.status);
      }

      chrome.test.succeed();
    });
  }

  chrome.tabs.create({
      url: rewriteURL('http://localhost:PORT/extensions/test_file.html')},
      function(tab) {
        testTabId = tab.id;
      });

  chrome.extension.onRequest.addListener(function(message) {
    chrome.test.assertEq('injected', message);

    chrome.test.runTests([
      // TODO(asargent): Explicitly create SSL test server and enable the test.
      // function disallowedSSL() {
      //   doReq('https://a.com', false);
      // },
      function targetPageAlwaysAllowed() {
        // Even though localhost does not show up in the host permissions, we
        // can still make requests to it since it's the page that the content
        // script is injected into.
        doReq('http://localhost', true);
      }
    ]);
  });
});
