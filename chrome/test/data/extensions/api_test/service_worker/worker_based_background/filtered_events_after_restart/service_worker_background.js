// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var TEST_DIR =
    '/extensions/api_test/service_worker/worker_based_background/' +
    'filtered_events_after_restart/';

function getURL(port, filename) {
  return 'http://127.0.0.1:' + port + TEST_DIR + filename;
}

function pass() { chrome.test.sendMessage('PASS_FROM_WORKER'); }
function fail() { chrome.test.sendMessage('FAIL_FROM_WORKER'); }

var seenURLs = [];
var recordCommitAndVerify = function(committedURL, filename) {
  var url = new URL(committedURL);
  if (url.pathname != TEST_DIR + filename) {
    fail();
    return;
  }
  seenURLs.push(url);

  if (seenURLs.length > 2) {
    fail();
    return;
  }
  if (seenURLs.length != 2) {
    return;
  }

  chrome.test.getConfig(function(config) {
    var port = config.testServer.port;
    var passed =
        seenURLs[0].href == getURL(port, 'a.html') &&
        seenURLs[1].href == getURL(port, 'b.html');
    passed ? pass() : fail();
  });
};

var registerFilteredEventListeners = function() {
  // We expect a.html to commit first and then b.html to commit.
  chrome.webNavigation.onCommitted.addListener(function(details) {
    fail();
  }, {url: [{pathSuffix: 'never-navigated.html'}]});
  chrome.webNavigation.onCommitted.addListener(function(details) {
    chrome.test.log('webNavigation.onCommitted, a.html: ' + details.url);
    recordCommitAndVerify(details.url, 'a.html');
  }, {url: [{pathSuffix: 'a.html'}]});
  chrome.webNavigation.onCommitted.addListener(function(details) {
    chrome.test.log('webNavigation.onCommitted, b.html: ' + details.url);
    recordCommitAndVerify(details.url, 'b.html');
  }, {url: [{pathSuffix: 'b.html'}]});
};

registerFilteredEventListeners();

chrome.test.sendMessage('ready');
