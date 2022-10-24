// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function addListeners() {
    chrome.webRequest.onBeforeRequest.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onBeforeSendHeaders.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onSendHeaders.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onHeadersReceived.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onBeforeRedirect.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onResponseStarted.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onCompleted.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onErrorOccurred.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.webRequest.onAuthRequired.addListener(
        function(details) {}, {urls: ["<all_urls>"]});
    chrome.test.succeed();
  },

  // Tests that we can remove a listener and it goes away.
  // http://crbug.com/96755
  function removeListeners() {
    function newCallback(value) {
      return function(details) { console.log(value); };
    }
    var cb1 = newCallback(1);
    var cb2 = newCallback(2);
    var event = chrome.webRequest.onBeforeRequest;
    event.addListener(cb1, {urls: ["<all_urls>"]});
    event.addListener(cb2, {urls: ["<all_urls>"]});
    chrome.test.assertTrue(event.hasListener(cb1));
    chrome.test.assertTrue(event.hasListener(cb2));
    event.removeListener(cb1);
    chrome.test.assertFalse(event.hasListener(cb1));
    chrome.test.assertTrue(event.hasListener(cb2));
    event.removeListener(cb2);
    chrome.test.assertFalse(event.hasListener(cb1));
    chrome.test.assertFalse(event.hasListener(cb2));
    event.addListener(cb1, {urls: ["http://foo/*"]});
    event.addListener(cb1, {urls: ["http://bar/*"]});
    event.removeListener(cb1);
    chrome.test.assertFalse(event.hasListener(cb1));
    chrome.test.succeed();
  },

  // Tests that the extra parameters to addListener are checked for invalid
  // values.
  function specialEvents() {
    var goodFilter = {urls: ["http://*.google.com/*"]};
    var goodExtraInfo = ["blocking"];
    chrome.webRequest.onBeforeRequest.addListener(
        function(details) {},
        goodFilter, goodExtraInfo);

    function isArgumentParseError(error) {
      // Native and JS bindings have slightly different errors surfaced.
      return error.search('Invalid value') >= 0 ||
             error.search('Error at parameter') >= 0;
    }

    // Try a bad RequestFilter.
    try {
      chrome.webRequest.onBeforeRequest.addListener(
          function(details) {},
          {badFilter: 42, urls: ["<all_urls>"]}, goodExtraInfo);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(isArgumentParseError(e.message), e.message);
    }

    // Try a bad ExtraInfoSpec.
    try {
      chrome.webRequest.onBeforeRequest.addListener(
          function(details) {},
          goodFilter, ["badExtraInfo"]);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(isArgumentParseError(e.message), e.message);
    }

    // This extraInfoSpec should only work for onBeforeSendHeaders.
    var headersExtraInfo = ["requestHeaders"];
    chrome.webRequest.onBeforeSendHeaders.addListener(
        function(details) {},
        goodFilter, headersExtraInfo);
    try {
      chrome.webRequest.onBeforeRequest.addListener(
          function(details) {},
          goodFilter, headersExtraInfo);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(isArgumentParseError(e.message), e.message);
    }

    // ExtraInfoSpec with "responseHeaders" should work for onCompleted.
    headersExtraInfo = ["responseHeaders"];
    chrome.webRequest.onCompleted.addListener(
        function(details) {},
        goodFilter, headersExtraInfo);
    try {
      chrome.webRequest.onBeforeRequest.addListener(
          function(details) {},
          goodFilter, headersExtraInfo);
      chrome.test.fail();
    } catch (e) {
      chrome.test.assertTrue(isArgumentParseError(e.message), e.message);
    }

    // Try a bad URL pattern. The error happens asynchronously. We're just
    // verifying that the browser doesn't crash.
    var emptyCallback = function (details) {};
    chrome.webRequest.onBeforeRequest.addListener(
        emptyCallback,
        {urls: ["badpattern://*"]});
    chrome.webRequest.onBeforeRequest.removeListener(
        emptyCallback);

    chrome.test.succeed();
  },

  // Checks that a header with NUL bytes does not cause a crash.
  // Regression test for http://crbug.com/348417
  function badResponseHeaderDoesNotCauseCrash() {
    var headerName = 'X-Header-With-Invalid-Value';
    var callbackWithBadHeadersResponse = function (details) {
      var responseHeaders = [
        {name: headerName, value: "\x00"}
      ];
      return {responseHeaders: responseHeaders};
    };
    chrome.webRequest.onHeadersReceived.addListener(
        callbackWithBadHeadersResponse,
        {types: ["xmlhttprequest"], urls: ["*://*/*"]},
        ["blocking", "responseHeaders"]);

    chrome.test.getConfig(function(config) {
      var url = 'http://127.0.0.1:' + config.testServer.port + '/simple.html';
      fetch(url).then((response) => {
        chrome.webRequest.onHeadersReceived.removeListener(
            callbackWithBadHeadersResponse);
        chrome.test.assertEq(undefined, response.headers.get(headerName));
        // TODO(robwu): If possible, check whether an error with the following
        // message has been logged to the JavaScript console:
        // "Header 'X-Header-With-Invalid-Value' has an invalid value"
        chrome.test.succeed();
      }).catch((e) => {
        chrome.webRequest.onHeadersReceived.removeListener(
            callbackWithBadHeadersResponse);
        chrome.test.fail(e);
      });
    });
  }
]);
