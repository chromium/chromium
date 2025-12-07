// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

let testUtil;

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
 * Handles removing an entry watcher.
 *
 * @param {AddWatcherRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback with an error code.
 */
function onRemoveWatcherRequested(options, onSuccess, onError) {
  if (options.fileSystemId !== testUtil.FILE_SYSTEM_ID) {
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
      testUtil.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onAddWatcherRequested.addListener(
      testUtil.onAddWatcherRequested);

  testUtil.defaultMetadata['/' + TESTING_FILE.name] = TESTING_FILE;
  testUtil.defaultMetadata['/' + TESTING_BROKEN_FILE.name] =
      TESTING_BROKEN_FILE;

  chrome.fileSystemProvider.onRemoveWatcherRequested.addListener(
      onRemoveWatcherRequested);

  testUtil.mountFileSystem(callback);
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([

    // Add and remove an entry watcher on an existing file.
    function removeWatcher() {
      testUtil.fileSystem.root.getFile(
          TESTING_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            // Add the watcher first, so there is something to remove.
            chrome.fileManagerPrivate.addFileWatch(
                fileEntry,
                chrome.test.callbackPass(function(result) {
                  chrome.test.assertTrue(result);
                  chrome.fileManagerPrivate.removeFileWatch(
                      fileEntry,
                      chrome.test.callbackPass(function(result) {
                        chrome.test.assertTrue(result);
                        chrome.fileSystemProvider.getAll(
                            chrome.test.callbackPass(function(items) {
                              chrome.test.assertEq(1, items.length);
                              chrome.test.assertEq(
                                  0, items[0].watchers.length);
                            }));
                        }));
                }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Remove a watcher which should not exist anymore, which should fail.
    // fail.
    function removeNonExistingFileWatcher() {
      testUtil.fileSystem.root.getFile(
          TESTING_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
            chrome.fileManagerPrivate.removeFileWatch(
                fileEntry,
                chrome.test.callbackFail(
                    'Unknown error.', function(result) {
                      chrome.test.assertFalse(!!result);
                    }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Add an entry watcher and tries removes it. The providing extension
    // returns an error, but the watcher should be removed anyway.
    function removeBrokenFileWatcher() {
      testUtil.fileSystem.root.getFile(
          TESTING_BROKEN_FILE.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_BROKEN_FILE.name, fileEntry.name);
            chrome.fileManagerPrivate.addFileWatch(
                fileEntry,
                chrome.test.callbackPass(function(result) {
                  chrome.test.assertTrue(result);
                  chrome.fileManagerPrivate.removeFileWatch(
                      fileEntry,
                      chrome.test.callbackPass(function(result) {
                        chrome.test.assertTrue(result);
                        chrome.fileSystemProvider.getAll(
                            chrome.test.callbackPass(function(items) {
                              chrome.test.assertEq(1, items.length);
                              chrome.test.assertEq(
                                  0, items[0].watchers.length);
                            }));
                    }));
                }));
          }), function(error) {
            chrome.test.fail(error.name);
          });
    }
  ]);
}

// This works-around that background scripts can't import because they aren't
// considered modules.
(async () => {
  testUtil = await import(
    '/_test_resources/api_test/file_system_provider/test_util.js');

  // Setup and run all of the test cases.
  setUp(runTests);
})();
