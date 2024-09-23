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
  name: 'nakameguro',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {string}
 * @const
 */
var TESTING_TAG1 = 'hello-puppy';

/**
 * @type {string}
 * @const
 */
var TESTING_TAG2 = 'hello-cat';

/**
 * @type {string}
 * @const
 */
var TESTING_TAG3 = 'hello-giraffe';

/**
 * List of directory changed events received from the chrome.fileManagerPrivate
 * API.
 * @type {Array<Object>}
 */
var directoryChangedEvents = [];

/**
 * Callback to be called when a directory changed event arrives.
 * @type {function()|null}
 */
var directoryChangedCallback = null;

/**
 * Handles an event dispatched from the chrome.fileManagerPrivate API.
 * @param {Object} event Event with the directory change details.
 */
function onDirectoryChanged(event) {
  directoryChangedEvents.push(event);
  chrome.test.assertTrue(!!directoryChangedCallback);
  directoryChangedCallback();
}

/**
 * Sets up the tests. Called once per all test cases. In case of a failure,
 * the callback is not called.
 *
 * @param {function()} callback Success callback.
 */
function setUp(callback) {
  chrome.fileManagerPrivate.onDirectoryChanged.addListener(onDirectoryChanged);

  chrome.fileSystemProvider.onGetMetadataRequested.addListener(
      test_util.onGetMetadataRequestedDefault);
  chrome.fileSystemProvider.onAddWatcherRequested.addListener(
      test_util.onAddWatcherRequested);
  chrome.fileSystemProvider.onRemoveWatcherRequested.addListener(
      test_util.onRemoveWatcherRequested);

  test_util.defaultMetadata['/' + TESTING_DIRECTORY.name] = TESTING_DIRECTORY;

  test_util.mountFileSystem(callback, {supportsNotifyTag: true});
}

/**
 * Runs all of the test cases, one by one.
 */
function runTests() {
  chrome.test.runTests([

    // Add a watcher, and then notifies that the entry has changed.
    function notifyChanged() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, fileEntry.name);
            test_util.toExternalEntry(fileEntry).then(
                chrome.test.callbackPass(function(externalEntry) {
                  chrome.test.assertTrue(!!externalEntry);
                  chrome.fileManagerPrivate.addFileWatch(
                      externalEntry,
                      chrome.test.callbackPass(function(result) {
                        chrome.test.assertTrue(result);
                        // Verify closure called when an event arrives.
                        directoryChangedCallback = chrome.test.callbackPass(
                            function() {
                              chrome.test.assertEq(
                                  1, directoryChangedEvents.length);
                              chrome.test.assertEq(
                                  'changed',
                                  directoryChangedEvents[0].eventType);
                              chrome.test.assertEq(
                                  externalEntry.toURL(),
                                  directoryChangedEvents[0].entry.toURL());
                              // Confirm that the tag is updated.
                              chrome.fileSystemProvider.getAll(
                                  chrome.test.callbackPass(function(items) {
                                    chrome.test.assertEq(1, items.length);
                                    chrome.test.assertEq(
                                        1, items[0].watchers.length);
                                    var watcher = items[0].watchers[0];
                                    chrome.test.assertEq(
                                        TESTING_TAG1, watcher.lastTag);
                                  }));
                            });
                        // TODO(mtomasz): Add more advanced tests, eg. for the
                        // details of changes.
                        chrome.fileSystemProvider.notify(
                            {
                              fileSystemId: test_util.FILE_SYSTEM_ID,
                              observedPath: fileEntry.fullPath,
                              recursive: false,
                              changeType: 'CHANGED',
                              changes: [{
                                entryPath: fileEntry.fullPath,
                                changeType: 'CHANGED',
                                cloudFileInfo: {
                                  versionTag: 'abc',
                                }
                              }],
                              tag: TESTING_TAG1
                            },
                            chrome.test.callbackPass());
                      }));
                })).catch(chrome.test.fail);
          }), function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Notifying with a null cloudFileInfo should succeed.
    function notifyEmptyCloudFileInfo() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name, {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, fileEntry.name);
            directoryChangedCallback = function() {};
            chrome.fileSystemProvider.notify(
                {
                  fileSystemId: test_util.FILE_SYSTEM_ID,
                  observedPath: fileEntry.fullPath,
                  recursive: false,
                  changeType: 'CHANGED',
                  // No cloudFileInfo.
                  changes: [{
                    entryPath: fileEntry.fullPath,
                    changeType: 'CHANGED',
                  }],
                  tag: TESTING_TAG2,
                },
                chrome.test.callbackPass());
          }));
    },

    // Passing an empty tag (or no tag) is invalid when the file system supports
    // the tag.
    function notifyEmptyTag() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, fileEntry.name);
            directoryChangedCallback = function() {
              chrome.test.fail();
            };
            // TODO(mtomasz): NOT_FOUND error should be returned instead.
            chrome.fileSystemProvider.notify({
              fileSystemId: test_util.FILE_SYSTEM_ID,
              observedPath: fileEntry.fullPath,
              recursive: false,
              changeType: 'CHANGED',
            }, chrome.test.callbackFail('INVALID_OPERATION'));
          }),
          function(error) {
            chrome.test.fail(error.name);
          });
    },

    // Notifying for the watched entry but in a wrong mode (recursive, while the
    // watcher is not recursive) should fail.
    function notifyWatchedPathButDifferentModeTag() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, fileEntry.name);
            directoryChangedCallback = function() {
              chrome.test.fail();
            };
            // TODO(mtomasz): NOT_FOUND error should be returned instead.
            chrome.fileSystemProvider.notify(
                {
                  fileSystemId: test_util.FILE_SYSTEM_ID,
                  observedPath: fileEntry.fullPath,
                  recursive: true,
                  changeType: 'CHANGED',
                  tag: TESTING_TAG3,
                },
                chrome.test.callbackFail('NOT_FOUND'));
          }));
    },

    // Notify about the watched entry being removed. That should result in the
    // watcher being removed.
    function notifyDeleted() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, fileEntry.name);
            // Verify closure called when an even arrives.
            test_util.toExternalEntry(fileEntry).then(
                chrome.test.callbackPass(function(externalEntry) {
                  chrome.test.assertTrue(!!externalEntry);
                  directoryChangedCallback =
                      chrome.test.callbackPass(function() {
                        chrome.test.assertEq(3, directoryChangedEvents.length);
                        chrome.test.assertEq(
                            'changed', directoryChangedEvents[1].eventType);
                        chrome.test.assertEq(
                            externalEntry.toURL(),
                            directoryChangedEvents[1].entry.toURL());
                        // Confirm that the watcher is removed.
                        chrome.fileSystemProvider.getAll(
                            chrome.test.callbackPass(function(fileSystems) {
                              chrome.test.assertEq(1, fileSystems.length);
                              chrome.test.assertEq(
                                  0, fileSystems[0].watchers.length);
                            }));
                      });
                  // TODO(mtomasz): Add more advanced tests, eg. for the details
                  // of changes.
                  chrome.fileSystemProvider.notify(
                      {
                        fileSystemId: test_util.FILE_SYSTEM_ID,
                        observedPath: fileEntry.fullPath,
                        recursive: false,
                        changeType: 'DELETED',
                        tag: TESTING_TAG3
                      },
                      chrome.test.callbackPass());
                })).catch(chrome.test.fail);
          }));
    },

    // Notify about an entry which is not watched. That should result in an
    // error.
    function notifyNotWatched() {
      test_util.fileSystem.root.getDirectory(
          TESTING_DIRECTORY.name,
          {create: false},
          chrome.test.callbackPass(function(fileEntry) {
            chrome.test.assertEq(TESTING_DIRECTORY.name, fileEntry.name);
            test_util.toExternalEntry(fileEntry).then(
                chrome.test.callbackPass(function(externalEntry) {
                  chrome.test.assertTrue(!!externalEntry);
                  directoryChangedCallback = function() {
                    chrome.test.fail();
                  };
                  // TODO(mtomasz): NOT_FOUND error should be returned instead.
                  chrome.fileSystemProvider.notify(
                      {
                        fileSystemId: test_util.FILE_SYSTEM_ID,
                        observedPath: fileEntry.fullPath,
                        recursive: false,
                        changeType: 'CHANGED',
                        tag: TESTING_TAG3
                      },
                      chrome.test.callbackFail('NOT_FOUND'));
                })).catch(chrome.test.fail);
            }));
    }
  ]);
}

// Setup and run all of the test cases.
setUp(runTests);
