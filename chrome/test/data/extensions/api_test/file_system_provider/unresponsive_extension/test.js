// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Id of the last created tab.
 * @type {number}
 */
var lastTabId = -1;

/**
 * Handles a configuration request and simulates a delayed success. Note, that
 * it should timeout, as the timeout for this test is set to 0 ms.
 */
function onConfigureRequested(options, onSuccess, onError) {
  setTimeout(onSuccess, 0);
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
    // Verify that if no window nor tab is opened, then the request will let
    // users abort the operation via notification.
    function unresponsiveWithoutUI() {
      chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
          chrome.test.callbackFail('Failed to complete configuration.',
              function() {}));
    },

    // Verify that if a tab is opened, then the request will not invoke
    // a notification.
    function unresponsiveWithTab() {
      chrome.tabs.create(
          {url: 'stub.html'},
          chrome.test.callbackPass(function(tab) {
            lastTabId = tab.id;
            chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
                chrome.test.callbackPass(function() {}))
          }));
    },

    // Verify that if a window is opened, then the request will not invoke
    // a notification.
    function unresponsiveWithWindow() {
      chrome.tabs.remove(lastTabId, chrome.test.callbackPass(function() {
        chrome.windows.create(
            {url: 'stub.html'},
            chrome.test.callbackPass(function(ignore) {
              chrome.fileManagerPrivate.configureVolume(test_util.volumeId,
                  chrome.test.callbackPass(function() {}))
            }));
      }));
    },
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
