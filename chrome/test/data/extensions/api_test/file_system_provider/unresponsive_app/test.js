// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Handles a configuration request and simulates a delayed success. Note, that
 * it should timeout, as the timeout for this test is set to 0 ms.
 */
function onConfigureRequested(options, onSuccess, onError) {
  setTimeout(onSuccess, 100);
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  test_util.mountFileSystem(callback);
  chrome.fileSystemProvider.onConfigureRequested.addListener(
      onConfigureRequested);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Verify that if no window is opened, then the request will let users abort
    // the operation via notification.
    function unresponsiveWithoutUI() {
      chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
          chrome.test.callbackFail('Failed to complete configuration.',
              function() {}));
    },

    // Verify that if a window is opened, then the request will not invoke
    // a notification.
    function unresponsiveWindow() {
      chrome.app.window.create(
          'stub.html',
          {},
          chrome.test.callbackPass(function(appWindow) {
            chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
                chrome.test.callbackPass(function() {}))
          }));
    }

  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
