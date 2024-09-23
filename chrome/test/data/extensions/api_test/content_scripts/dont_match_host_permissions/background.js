// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var receivedRequests = {};

chrome.runtime.onMessage.addListener(function(request, sender, sendResponse) {
  if (receivedRequests[request.source]) {
    chrome.test.fail(
        'Received multiple requests from "' + request.source + '".');
    return;
  }

  chrome.test.assertEq(request.source == 'a.com', request.modified);
  receivedRequests[request.source] = true;
  if (receivedRequests['a.com'] && receivedRequests['b.com'])
    chrome.test.succeed();
});

// We load two pages. On a.com, both our modify and test script will run and we
// will receive a request that says that the page was modified. On b.com, only
// the test script will run, and the request will say that the page was not
// modified.
chrome.test.getConfig(function(config) {
  chrome.tabs.create({
      url: 'http://a.com:' + config.testServer.port +
           '/extensions/test_file.html'});
  chrome.tabs.create({
      url: 'http://b.com:' + config.testServer.port +
           '/extensions/test_file.html'});
});
