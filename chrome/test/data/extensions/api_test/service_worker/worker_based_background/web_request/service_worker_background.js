// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabUrl;

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      tabUrl = `http://a.com:${config.testServer.port}/extensions` +
          '/api_test/service_worker/worker_based_background/web_request/' +
          'empty.html';
      chrome.test.succeed();
    });
  },
  function testOnBeforeRequest() {
    chrome.webRequest.onBeforeRequest.addListener(
        function localListener(details) {
          chrome.webRequest.onBeforeRequest.removeListener(localListener);
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        }, { urls: ['<all_urls>']});
    // Create the tab.
    chrome.tabs.create({url: tabUrl});
  },
  function testOnBeforeSendHeaders() {
    chrome.webRequest.onBeforeSendHeaders.addListener(
        function localListener(details) {
          chrome.webRequest.onBeforeSendHeaders.removeListener(localListener);
          chrome.test.assertNoLastError();
          chrome.test.assertEq(tabUrl, details.url);
          chrome.test.succeed();
        }, { urls: [tabUrl]});
    chrome.tabs.create({url: tabUrl});
  },
  function testOnSendHeaders() {
    chrome.webRequest.onSendHeaders.addListener(
        function localListener(details) {
          chrome.webRequest.onSendHeaders.removeListener(localListener);
          chrome.test.assertNoLastError();
          chrome.test.assertEq(tabUrl, details.url);
          chrome.test.succeed();
          }, { urls: [tabUrl]});
    chrome.tabs.create({url: tabUrl});
  },
  function testOnHeadersReceived() {
    chrome.webRequest.onHeadersReceived.addListener(
        function localListener(details) {
          chrome.webRequest.onHeadersReceived.removeListener(localListener);
          chrome.test.assertNoLastError();
          chrome.test.assertEq(tabUrl, details.url);
          chrome.test.succeed();
        }, {urls: [tabUrl]});
    chrome.tabs.create({url: tabUrl});
  },
  function testOnResponseStarted() {
    chrome.webRequest.onResponseStarted.addListener(
        function localListener(details) {
          chrome.webRequest.onResponseStarted.removeListener(localListener);
          chrome.test.assertNoLastError();
          chrome.test.assertEq(tabUrl, details.url);
          chrome.test.succeed();
        }, {urls: [tabUrl]});
    chrome.tabs.create({url: tabUrl});
  },
  function testOnCompleted() {
    chrome.webRequest.onCompleted.addListener(
        function localListener(details) {
          chrome.webRequest.onCompleted.removeListener(localListener);
          chrome.test.assertNoLastError();
          chrome.test.assertEq(tabUrl, details.url);
          chrome.test.succeed();
        }, {urls: [tabUrl]});
    chrome.tabs.create({url: tabUrl});
  },
]);
