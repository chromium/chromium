// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var url = new URL(location.href).searchParams.get('url');
var filter = {urls: [url], types: ["xmlhttprequest"]};

chrome.webRequest.onCompleted.addListener(function(details) {
  chrome.test.notifyFail("Request completed unexpectedly");
}, filter);

chrome.webRequest.onErrorOccurred.addListener(function(details) {
  chrome.test.assertEq(-1, details.tabId);
  chrome.test.assertEq(url, details.url);
  chrome.test.assertEq("xmlhttprequest", details.type);
  chrome.test.assertEq("net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED", details.error);

  chrome.test.notifyPass();
}, filter);
