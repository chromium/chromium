// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessage.addListener(function(message, sender, sendResponse) {
  chrome.test.assertEq('msg from tab', message);
  sendResponse('Reply here');
});

var port = chrome.runtime.connect();
port.postMessage('Hello from content script');
port.disconnect();
