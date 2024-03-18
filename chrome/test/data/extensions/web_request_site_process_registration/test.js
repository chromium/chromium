// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webRequest.onBeforeRequest.addListener(
  function(details) {
    if (details.url == chrome.runtime.getURL("/blocked.html")) {
      return {cancel: true};
    }
  },
  {urls: ["<all_urls>"]},
  ["blocking"]
);
