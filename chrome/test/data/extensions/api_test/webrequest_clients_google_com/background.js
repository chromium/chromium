// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.clientsGoogleWebRequestCount = 0;
self.yahooWebRequestCount = 0;

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  if (details.url.includes('http://clients1.google.com/')) {
    ++self.clientsGoogleWebRequestCount;
  } else if (details.url.includes('http://yahoo.com')) {
    ++self.yahooWebRequestCount;
  }
}, {urls: ['<all_urls>']});

chrome.test.sendMessage('ready');
