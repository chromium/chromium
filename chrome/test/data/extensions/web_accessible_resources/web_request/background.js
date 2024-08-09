// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webRequest.onBeforeRequest.addListener(details => {
  if (details.url.endsWith('redirect.js')) {
    return {redirectUrl: chrome.runtime.getURL('war.js')};
  }
}, {urls: ['<all_urls>']}, ['blocking']);

chrome.test.sendMessage('ready');
