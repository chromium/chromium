// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var requests = [];
chrome.webRequest.onBeforeRequest.addListener(function(details) {
  // Show the query string of the request. If this is absent for some
  // reason (probably an error), show the full URL.
  requests.push(new URL(details.url).search || details.url);
}, {
  types: ['xmlhttprequest'],
  urls: ['*://*/*']
});

chrome.test.sendMessage('web_request_status1', echoRequestStatus);
chrome.test.sendMessage('web_request_status2', echoRequestStatus);

function echoRequestStatus() {
  if (requests.length === 0) {
    chrome.test.sendMessage('Did not intercept any requests.');
  } else {
    chrome.test.sendMessage('Intercepted requests: ' + requests.join(', '));
  }
  requests.length = 0;
}
