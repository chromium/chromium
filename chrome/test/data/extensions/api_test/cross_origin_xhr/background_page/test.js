// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {

  function rewriteURL(url) {
    return url.replace(/PORT/, config.testServer.port);
  }

  function doReq(domain, expectSuccess) {
    var req = new XMLHttpRequest();
    var url = rewriteURL(domain + ":PORT/extensions/test_file.txt");
    var isErrorTriggered = false;

    chrome.test.log("Requesting url: " + url);
    req.open("GET", url, true);


    if (expectSuccess) {
      req.onload = function() {
        if (/^https?:/i.test(url))
          chrome.test.assertEq(200, req.status);
        chrome.test.assertEq("Hello!", req.responseText);
        chrome.test.succeed();
      }
      req.onerror = function() {
        isErrorTriggered = true;
        chrome.test.log("status: " + req.status);
        chrome.test.log("text: " + req.responseText);
        chrome.test.fail("Unexpected error for domain: " + domain);
      }
    } else {
      req.onload = function() {
        chrome.test.fail("Unexpected success for domain: " + domain);
      }
      req.onerror = function() {
        isErrorTriggered = true;
        chrome.test.assertEq(0, req.status);
        chrome.test.succeed();
      }
    }

    try {
      req.send(null);
    } catch (e) {
      if (/^https?:/i.test(url)) {
        chrome.test.fail(
                "req.send() has thrown an error for " + domain + ": " + e);
      } else if (!isErrorTriggered) {
        // A NetworkError will synchronously be be thrown whenever a
        // FTP request fails. This should be handled by req.onerror.
        chrome.test.fail("req.send() has thrown an error without dispatching " +
                         "the req.onerror event for " + domain + ": " + e);
      }
    }
  }

  chrome.test.runTests([
    function allowedOrigin() {
      doReq("http://a.com", true);
    },
    function diallowedOrigin() {
      doReq("http://c.com", false);
    },
    function allowedSubdomain() {
      doReq("http://foo.b.com", true);
    },
    function noSubdomain() {
      doReq("http://b.com", true);
    },
    function disallowedSubdomain() {
      doReq("http://foob.com", false);
    },
    // TODO(asargent): Explicitly create SSL test server and enable the test.
    // function disallowedSSL() {
    //   doReq("https://a.com", false);
    // }
  ]);
});
