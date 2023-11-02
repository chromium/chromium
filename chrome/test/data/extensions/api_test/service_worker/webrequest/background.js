// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.webRequest.onBeforeRequest.addListener(onBeforeRequest,
                                              {urls:['*://*/*hello.txt*']});

let lastHookedRequestUrl;

function onBeforeRequest(details) {
  lastHookedRequestUrl = new URL(details.url);
}

// Returns path of the latest URL recorded in onBeforeRequest()
function getLastHookedPath() {
  const path = lastHookedRequestUrl.pathname + lastHookedRequestUrl.search;
  window.domAutomationController.send(path);
}
