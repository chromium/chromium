// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var url = new URL(location.href).searchParams.get('url');
var filter = {urls: [url], types: ["xmlhttprequest"]};

chrome.webRequest.onCompleted.addListener(function(details) {
  chrome.test.assertEq(-1, details.tabId);
  chrome.test.assertEq(url, details.url);
  chrome.test.assertEq("xmlhttprequest", details.type);
  chrome.test.assertEq(401, details.statusCode);

  chrome.test.notifyPass();
}, filter);

chrome.webRequest.onErrorOccurred.addListener(function(details) {
  chrome.test.notifyFail("Request failed");
}, filter);
