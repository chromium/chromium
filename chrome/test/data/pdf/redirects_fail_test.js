// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

// Load a plugin with the given parameters.
function createPluginForUrl(streamUrl, url, progressCallback) {
  const plugin = document.createElement('embed');
  plugin.type = 'application/x-google-chrome-pdf';
  plugin.addEventListener('message', function(message) {
    switch (message.data.type.toString()) {
      case 'loadProgress':
        progressCallback(message.data.progress);
        break;
    }
  }, false);

  plugin.setAttribute('original-url', url);
  plugin.setAttribute('src', streamUrl);
  plugin.setAttribute('full-frame', '');
  document.body.appendChild(plugin);
}

const tests = [
  // Test that if the plugin is loaded with a URL that redirects it fails.
  function redirectsFail() {
    const base = new URL(viewer.originalUrl);
    const redirectUrl = new URL('/server-redirect?' + viewer.originalUrl, base);
    createPluginForUrl(redirectUrl, redirectUrl, function(progress) {
      if (progress === -1) {
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    });
  },

  // Test that if the plugin is loaded with a URL that doesn't redirect, it
  // succeeds.
  function noRedirectsSucceed() {
    createPluginForUrl(
        viewer.originalUrl, viewer.originalUrl, function(progress) {
          if (progress === 100) {
            chrome.test.succeed();
          }
        });
  },
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCompleteCallback(function() {
  chrome.test.runTests(tests);
});
