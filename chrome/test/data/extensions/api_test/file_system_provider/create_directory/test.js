// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_DIRECTORY = Object.freeze({
  isDirectory: true,
  name: 'kitty',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * Creates a directory.
 *
 * @param {CreateDirectoryRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback
 * @param {function(string)} onError Error callback with an error code.
 */
function onCreateDirectoryRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.directoryPath === '/' || options.recursive) {
    onError('INVALID_OPERATION');
    return;
  }

  if (options.directoryPath in test_util.defaultMetadata) {
    onError('EXISTS');
    return;
  }

  test_util.defaultMetadata[options.directoryPath] = TESTING_DIRECTORY;
  onSuccess();  // enum ProviderError.
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);

  test_util.defaultMetadata['/' + TESTING_DIRECTORY.name] =
      TESTING_DIRECTORY;

  chrome.fileSystemProvider.onCreateDirectoryRequested.addListener(
      onCreateDirectoryRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Create a directory (not exclusive). Should succeed.
    function createDirectorySuccessSimple() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name, {create: true, exclusive: false},
          chrome.test.callbackPass(function(entry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, entry.name);
            chrome.test.assertTrue(entry.isDirectory);
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Create a directory (exclusive). Should fail, since the directory already
    // exists.
    function createDirectoryErrorExists() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name, {create: true, exclusive: true},
          function(entry) {
            chrome.test.fail('Created a directory, but should fail.');
          }, chrome.test.callbackPass(function(error) {
            chrome.test.assertEq('InvalidModificationError', error.name);
          }));
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
