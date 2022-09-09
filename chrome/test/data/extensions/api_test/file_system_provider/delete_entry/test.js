// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_A_DIRECTORY = Object.freeze({
  isDirectory: true,
  name: 'a',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_B_DIRECTORY = Object.freeze({
  isDirectory: true,
  name: 'b',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_C_FILE = Object.freeze({
  isDirectory: false,
  name: 'c',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * Deletes an entry.
 *
 * @param {DeleteEntryRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback
 * @param {function(string)} onError Error callback with an error code.
 */
function onDeleteEntryRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath === '/') {
    onError('INVALID_OPERATION');
    return;
  }

  if (options.entryPath === '/' + TESTING_A_DIRECTORY.name) {
    if (options.recursive)
      onSuccess();
    else
      onError('INVALID_OPERATION');
    return;
  }

  if (options.entryPath === '/' + TESTING_C_FILE.name ||
      options.entryPath === '/' + TESTING_A_DIRECTORY.name + '/' +
      TESTING_B_DIRECTORY.name) {
    onSuccess();
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
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

  test_util.defaultMetadata['/' + TESTING_A_DIRECTORY.name] =
      TESTING_A_DIRECTORY;
  test_util.defaultMetadata['/' + TESTING_A_DIRECTORY.name + '/' +
      TESTING_B_DIRECTORY.name] = TESTING_B_DIRECTORY;
  test_util.defaultMetadata['/' + TESTING_C_FILE.name] =
      TESTING_C_FILE;

  chrome.fileSystemProvider.onDeleteEntryRequested.addListener(
      onDeleteEntryRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([
    // Delete a file. Should succeed.
    function deleteDirectorySuccessSimple() {
      test_util.fileSystem.root.getFile(
          TESTING_C_FILE.name, {create: false},
          chrome.test.callbackPass(function(entry) {
            chrome.test.assertEq(TESTING_C_FILE.name, entry.name);
            chrome.test.assertFalse(entry.isDirectory);
            entry.remove(chrome.test.callbackPass(), function(error) {
              chrome.test.fail(error.name);
            });
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },
    // Delete a directory which has contents, non-recursively. Should fail.
    function deleteDirectoryErrorNotEmpty() {
      test_util.fileSystem.root.getDirectory(
          TESTING_A_DIRECTORY.name, {create: false},
          chrome.test.callbackPass(function(entry) {
            chrome.test.assertEq(TESTING_A_DIRECTORY.name, entry.name);
            chrome.test.assertTrue(entry.isDirectory);
            entry.remove(function() {
              chrome.test.fail('Unexpectedly succeded to remove a directory.');
            }, chrome.test.callbackPass);
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },
    // Delete a directory which has contents, recursively. Should succeed.
    function deleteDirectoryRecursively() {
      test_util.fileSystem.root.getDirectory(
          TESTING_A_DIRECTORY.name, {create: false},
          chrome.test.callbackPass(function(entry) {
            chrome.test.assertEq(TESTING_A_DIRECTORY.name, entry.name);
            chrome.test.assertTrue(entry.isDirectory);
            entry.removeRecursively(
                chrome.test.callbackPass(),
                function(error) {
                  chrome.test.fail(error);
                });
          }), function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
