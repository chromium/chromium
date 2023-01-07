// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
self.requestsIntercepted = 0;

chrome.webRequest.onBeforeRequest.addListener((details) => {
  self.requestsIntercepted++;
  chrome.test.sendMessage(details.initiator);
}, {urls: ['*://*/extensions/api_test/webrequest/xhr/data.json']}, []);

chrome.test.sendMessage('ready');
