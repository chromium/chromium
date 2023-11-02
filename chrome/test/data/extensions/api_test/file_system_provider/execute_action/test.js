// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_ACTIONS_DIR = Object.freeze({
  isDirectory: true,
  name: 'actions',
  size: 0,
  modificationTime: new Date(2014, 3, 27, 9, 38, 14)
});

/**
 * @type {string}
 * @const
 */
var TESTING_ACTION_ID = "testing-action";

/**
 * @type {string}
 * @const
 */
var TESTING_UNKNOWN_ACTION_ID = "testing-unknown-action";

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);

  test_util.defaultMetadata['/' + TESTING_ACTIONS_DIR.name] =
      TESTING_ACTIONS_DIR;

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Executes an existing action.
    function executeActionSuccess() {
      var onExecuteActionRequested = chrome.test.callbackPass(
          function(options, onSuccess, onError) {
            chrome.test.assertEq(test_util.FILE_SYSTEM_ID,
                options.fileSystemId);
            chrome.test.assertEq(1, options.entryPaths.length);
            chrome.test.assertEq('/' + TESTING_ACTIONS_DIR.name,
                options.entryPaths[0]);
            chrome.test.assertEq(TESTING_ACTION_ID, options.actionId);
            chrome.fileSystemProvider.onExecuteActionRequested.removeListener(
                onExecuteActionRequested);
            onSuccess();
          });
      chrome.fileSystemProvider.onExecuteActionRequested.addListener(
          onExecuteActionRequested);
      test_util.fileSystem.root.getDirectory(
          TESTING_ACTIONS_DIR.name,
          {create: false},
          chrome.test.callbackPass(function(dirEntry) {
            chrome.fileManagerPrivate.executeCustomAction(
                [dirEntry],
                TESTING_ACTION_ID,
                chrome.test.callbackPass(function() {}));
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Tries to execute a non-existing action.
    function executeNonExistingActionFailure() {
      var onExecuteActionRequested = chrome.test.callbackPass(
          function(options, onSuccess, onError) {
            chrome.test.assertEq(test_util.FILE_SYSTEM_ID,
                options.fileSystemId);
            chrome.test.assertEq(1, options.entryPaths.length);
            chrome.test.assertEq('/' + TESTING_ACTIONS_DIR.name,
                options.entryPaths[0]);
            chrome.test.assertEq(TESTING_UNKNOWN_ACTION_ID, options.actionId);
            chrome.fileSystemProvider.onExecuteActionRequested.removeListener(
                onExecuteActionRequested);
            onError('NOT_FOUND');
          });
      chrome.fileSystemProvider.onExecuteActionRequested.addListener(
          onExecuteActionRequested);
      test_util.fileSystem.root.getDirectory(
          TESTING_ACTIONS_DIR.name,
          {create: false},
          chrome.test.callbackPass(function(dirEntry) {
            chrome.fileManagerPrivate.executeCustomAction(
                [dirEntry],
                TESTING_UNKNOWN_ACTION_ID,
                chrome.test.callbackFail('Failed to execute the action.',
                    function() {}));
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
