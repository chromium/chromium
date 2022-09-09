// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {Object}
 * @const
 */
var TESTING_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
var TESTING_BROKEN_FILE = Object.freeze({
  isDirectory: false,
  name: 'broken-file.txt',
  size: 4096,
  modificationTime: new Date(2014, 4, 27, 10, 38, 10)
});

/**
 * Handles adding a new entry watcher.
 *
 * @param {AddWatcherRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback with an error code.
 */
function onAddWatcherRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath === '/' + TESTING_FILE.name) {
    onSuccess();
    return;
  }

  if (options.entryPath === '/' + TESTING_BROKEN_FILE.name) {
    onError('INVALID_OPERATION');
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

  test_util.defaultMetadata['/' + TESTING_FILE.name] = TESTING_FILE;
  test_util.defaultMetadata['/' + TESTING_BROKEN_FILE.name] =
      TESTING_BROKEN_FILE;

  chrome.fileSystemProvider.onAddWatcherRequested.addListener(
      onAddWatcherRequested);

  test_util.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([

    // Add an entry watcher on an existing file.
    function addWatcher() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            chrome.fileManagerPrivate.addFileWatch(
                fileEntry,
                chrome.test.callbackPass(function(result) {
                  chrome.test.assertTrue(result);
                  chrome.fileSystemProvider.getAll(
                      chrome.test.callbackPass(function(fileSystems) {
                        chrome.test.assertEq(1, fileSystems.length);
                        chrome.test.assertEq(
                            1, fileSystems[0].watchers.length);
                        var watcher = fileSystems[0].watchers[0];
                        chrome.test.assertEq(
                            '/' + TESTING_FILE.name, watcher.entryPath);
                        chrome.test.assertFalse(watcher.recursive);
                        chrome.test.assertEq(undefined, watcher.tag);
                      }));
                }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Add an entry watcher on a file which is already watched, what should
    // fail.
    function addExistingFileWatcher() {
      test_util.fileSystem.root.getFile(
          TESTING_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            chrome.fileManagerPrivate.addFileWatch(
                fileEntry,
                chrome.test.callbackFail(
                    'Unknown error.', function(result) {
                      chrome.test.assertFalse(!!result);
                      chrome.fileSystemProvider.getAll(
                          chrome.test.callbackPass(function(fileSystems) {
                            chrome.test.assertEq(1, fileSystems.length);
                            chrome.test.assertEq(
                                1, fileSystems[0].watchers.length);
                          }));
                }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Add an entry watcher on a broken file, what should fail.
    function addBrokenFileWatcher() {
      test_util.fileSystem.root.getFile(
          TESTING_BROKEN_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_BROKEN_FILE.name, fileEntry.name);
            chrome.fileManagerPrivate.addFileWatch(
                fileEntry,
                chrome.test.callbackFail(
                    'Unknown error.', function(result) {
                      chrome.test.assertFalse(!!result);
                      chrome.fileSystemProvider.getAll(
                          chrome.test.callbackPass(function(fileSystems) {
                            chrome.test.assertEq(1, fileSystems.length);
                            chrome.test.assertEq(
                                1, fileSystems[0].watchers.length);
                          }));
                    }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
