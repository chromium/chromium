// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (_launchData) {
  chrome.app.window.create(
    'index.html',
    { width: 1920, height: 1080 },
    function (_window) { }
  );
});

chrome.runtime.onMessageExternal.addListener(
  function (request, _sender, sendResponse) {
    if (request === 'GET_APP_VERSION')
      sendResponse({ version: chrome.runtime.getManifest().version });
  });

