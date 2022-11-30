// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

self.pacRequestCount = 0;
self.title2RequestCount = 0;

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  ++self.pacRequestCount;
}, {urls: ['*://*/self.pac']});

chrome.webRequest.onBeforeRequest.addListener(function(details) {
  ++self.title2RequestCount;
}, {urls: ['*://*/title2.html']});

chrome.test.sendMessage('ready');
