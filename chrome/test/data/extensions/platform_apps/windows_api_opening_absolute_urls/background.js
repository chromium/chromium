// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openChromeUrl() {
    // The app is loaded as a component, so it should be able to load
    // chrome:-scheme URLs.
    chrome.app.window.create('chrome://version', (win) => {
      // The returned `win` here is null because it's inaccessible (by design)
      // by the Chrome app. But there should be no last error, signaling that
      // the call succeeded. The C++ will verify the presence of the opened tab
      // after the app finishes running.
      chrome.test.assertFalse(!!win);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
  function openFileUrlFails() {
    // Attempting to opening a file URL should fail. https://crbug.com/1276046.
    chrome.test.getConfig((config) => {
      // The customArg provided by the C++ points to a file URL in the test
      // data directory.
      chrome.app.window.create(config.customArg, (win) => {
        chrome.test.assertFalse(!!win);
        // TODO(devlin): This error message isn't entirely accurate (file URLs
        // are local). It should probably say something like "must point to a
        // resource contained by this app" or something.
        chrome.test.assertLastError(
            'The URL used for window creation must be local ' +
            'for security reasons.');
        chrome.test.succeed();
      });
    });
  },
]);
