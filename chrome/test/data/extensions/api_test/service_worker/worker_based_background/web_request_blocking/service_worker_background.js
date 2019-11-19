// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Register a listener with a callback that blocks all requests.
chrome.webRequest.onBeforeRequest.addListener(function localListener(details) {
  return {cancel: true};
}, { urls: ['<all_urls>']}, ['blocking']);

// Tell the C++ side of things to proceed with the test.
chrome.test.sendMessage('ready');
