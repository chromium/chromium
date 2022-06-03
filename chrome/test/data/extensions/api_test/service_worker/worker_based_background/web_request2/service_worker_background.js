// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabUrl;

chrome.test.runTests([
  function setup() {
    chrome.test.getConfig(function(config) {
      tabUrl = `http://a.com:${config.testServer.port}/extensions` +
          '/api_test/service_worker/worker_based_background/web_request2/' +
          'empty.html';
      chrome.test.succeed();
    });
  },
  function testOnBeforeRequestBlocked() {
    chrome.webRequest.onErrorOccurred.addListener(
        function localListener(details) {
          chrome.webRequest.onErrorOccurred.removeListener(localListener);
          chrome.test.assertEq('net::ERR_BLOCKED_BY_CLIENT', details.error);
          chrome.test.succeed();
        }, {urls: [tabUrl]});
    chrome.webRequest.onBeforeRequest.addListener(
        function localListener(details) {
          chrome.webRequest.onBeforeRequest.removeListener(localListener);
          return {cancel: true};
        }, {urls: [tabUrl]}, ['blocking']);
    chrome.tabs.create({url: tabUrl});
  },
]);
