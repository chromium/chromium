// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.webRequestCount = 0;
self.requestedHostnames = [];

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  ++self.webRequestCount;
  self.requestedHostnames.push((new URL(details.url)).hostname);
}, {urls:['<all_urls>']});

chrome.test.sendMessage('ready');
