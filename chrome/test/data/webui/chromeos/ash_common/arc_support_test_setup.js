// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Stubs Chrome app runtime APIs before loading arc_support/background.js in
// WebUI browser_tests.
window.arcSupportTestHarness = {
  launchedListener: null,
  sentMessages: [],
  nativeMessageListeners: [],
};

chrome.app = chrome.app || {};
chrome.app.runtime = {
  onLaunched: {
    addListener(fn) {
      window.arcSupportTestHarness.launchedListener = fn;
    },
  },
};

chrome.app.window = {
  create(_url, _options, callback) {
    if (window.arcSupportTestHarness.createWindowImpl) {
      window.arcSupportTestHarness.createWindowImpl(callback);
    }
  },
};

chrome.runtime = chrome.runtime || {};
chrome.runtime.connectNative = function(_hostName) {
  return {
    postMessage(msg) {
      window.arcSupportTestHarness.sentMessages.push(msg);
    },
    disconnect() {
      window.arcSupportTestHarness.disconnected = true;
    },
    onMessage: {
      addListener(fn) {
        window.arcSupportTestHarness.nativeMessageListeners.push(fn);
      },
    },
  };
};
