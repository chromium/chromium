// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Promise of the volume list.
 * @type {Promise}
 */
var volumeListPromise = new Promise(function(fulfill, reject) {
  chrome.fileManagerPrivate.getVolumeMetadataList(fulfill);
});

/**
 * Obtains file system of the volume type.
 * @param {string} volumeType VolumeType.
 * @return {Promise} Promise to be fulfilled with a file system.
 */
function getFileSystem(volumeType) {
  return volumeListPromise.then(function(list) {
    for (var i = 0; i < list.length; i++) {
      if (list[i].volumeType == volumeType) {
        return new Promise(function(fulfill) {
          chrome.fileSystem.requestFileSystem(
              {volumeId: list[i].volumeId, writable: true}, fulfill);
        });
      }
    }
    throw new Error('The volume is not found: ' + volumeType + '.');
  });
}

/**
 * Prepares a file on the file system.
 * @param {FileSystem} filesystem File system.
 * @param {string} name Name of the file.
 * @param {Blob} contents Contents of the file.
 * @return {Promise} Promise to be fulfilled with FileEntry of the new file.
 */
function prepareFile(filesystem, name, contents) {
  return new Promise(function(fulfill, reject) {
    filesystem.root.getFile(name, {create: true}, function(file) {
      file.createWriter(function(writer) {
        writer.write(contents);
        writer.onwrite = fulfill.bind(null, file);
        writer.onerror = reject;
      });
      fulfill(file);
    }, reject);
  });
}

/**
 * Prepares a directory on the file system.
 * @param {FileSystem} filesystem File system.
 * @param {string} name Name of the directory.
 * @param {Blob} contents Contents of the file.
 * @return {Promise} Promise to be fulfilled with DirectoryEntry of the new
 *     directory.
 */
function prepareDirectory(filesystem, name) {
  return new Promise(function(fulfill, reject) {
    filesystem.root.getDirectory(name, {create: true}, function(dirEntry) {
      fulfill(dirEntry);
    }, reject);
  });
}

/**
 * Prepares two test files on the file system.
 * @param {FileSystem} filesystem File system.
 * @return {Promise} Promise to be fullfilled with an object {filesystem:
 *     FileSystem, entries: Array<FileEntry>} that contains the passed file
 *     system and the created entries.
 */
function prepareFiles(filesystem) {
  var testFileA =
      prepareFile(filesystem, 'test_file_a.txt', TEST_FILE_CONTENTS);
  var testFileB =
      prepareFile(filesystem, 'test_file_b.txt', TEST_FILE_CONTENTS);
  return Promise.all([testFileA, testFileB]).then(function(entries) {
    return {filesystem: filesystem, entries: entries};
  });
}

/**
 * Prepares two test directories on the file system.
 * @param {FileSystem} filesystem File system.
 * @return {Promise} Promise to be fullfilled with an object {filesystem:
 *     FileSystem, entries: Array<DirectoryEntry>} that contains the passed file
 *     system and the created entries.
 */
function prepareDirectories(filesystem) {
  var testDirA = prepareDirectory(filesystem, 'dir1');
  var testDirB = prepareDirectory(filesystem, 'dir2');
  return Promise.all([testDirA, testDirB]).then(function(entries) {
    return {filesystem: filesystem, entries: entries};
  });
}

/**
 * Contents of the test file.
 * @type {Blob}
 * @const
 */
var TEST_FILE_CONTENTS = new Blob(['This is a test file.']);

/**
 * File system of the drive volume for files.
 * @type {Promise}
 */
var driveFileSystemPromise = getFileSystem('drive').then(prepareFiles);

/**
 * File system of the local volume for files.
 * @type {Promise}
 */
var localFileSystemPromise = getFileSystem('testing').then(prepareFiles);

/**
 * File system of the drive volume for directories.
 * @type {Promise}
 */
var driveDirSystemPromise = getFileSystem('drive').then(prepareDirectories);

/**
 * File system of the local volume for directories.
 * @type {Promise}
 */
var localDirSystemPromise = getFileSystem('drive').then(prepareDirectories);

/**
 * Calls test functions depends on the result of the promise.
 * @param {Promise} promise Promise to be fulfilled or to be rejected depends on
 *     the test results.
 */
function testPromise(promise) {
  promise.then(
      chrome.test.callbackPass(),
      function(error) {
        chrome.test.fail(error.stack || error);
      });
}

/**
 * Calls the executeTask API with the entries and checks the launch data passed
 * to onLaunched events.
 * @param {Array<Entry>} isolatedEntries Entries to be tested.
 * @return {Promise} Promise to be fulfilled on success.
 */
function launchWithEntries(isolatedEntries) {
  // TODO(mtomasz): Remove this hack once chrome.FileManager API can work on
  // isolated entries.
  return new Promise(
      function(fulfill, reject) {
        chrome.fileManagerPrivate.resolveIsolatedEntries(
            isolatedEntries,
            function(entries) {
              fulfill(entries);
            });
      })
      .then(
          function(entries) {
            var tasksPromise = new Promise(function(fulfill) {
              chrome.fileManagerPrivate.getFileTasks(entries, fulfill);
            }).then(function(tasks) {
              chrome.test.assertEq(1, tasks.length);
              chrome.test.assertEq("ChromeOS File handler extension",
                                   tasks[0].title);
              chrome.test.assertEq(
                  'pkplfbidichfdicaijlchgnapepdginl|app|textAction',
                  tasks[0].taskId);
              return tasks[0];
            });
            var launchDataPromise = new Promise(function(fulfill) {
              chrome.app.runtime.onLaunched.addListener(
                  function handler(launchData) {
                    chrome.app.runtime.onLaunched.removeListener(handler);
                    fulfill(launchData);
                  });
            });
            var taskExecutedPromise = tasksPromise.then(function(task) {
              return new Promise(function(fulfill, reject) {
                chrome.fileManagerPrivate.executeTask(
                    task.taskId,
                    entries,
                    function(result) {
                      if (result)
                        fulfill();
                      else
                        reject();
                    });
                });
            });
            var resolvedEntriesPromise = launchDataPromise.then(
                function(launchData) {
                  var entries = launchData.items.map(
                      function(item) { return item.entry; });
                  return new Promise(function(fulfill) {
                    chrome.fileManagerPrivate.resolveIsolatedEntries(
                        entries, fulfill);
                  });
                });
            return Promise.all([
              taskExecutedPromise,
              launchDataPromise,
              resolvedEntriesPromise
            ]).then(function(args) {
              chrome.test.assertEq(entries.length, args[1].items.length);
              chrome.test.assertEq(
                  entries.map(entry => entry.name).sort(),
                  args[1].items.map(item => item.entry.name).sort(),
                  'Wrong entries are passed to the application handler.');
              chrome.test.assertEq(
                  entries.map(entry => entry.toURL()).sort(),
                  args[2].map(entry => entry.toURL()).sort(),
                  'Entries passed to the application handler cannot be ' +
                      'resolved.');
            })
          });
}

/**
 * Tests the file handler feature with entries on the local volume.
 */
function testForLocalFiles() {
  testPromise(localFileSystemPromise.then(function(volume) {
    return launchWithEntries(volume.entries);
  }));
}

/**
 * Tests the file handler feature with entries on the drive volume.
 */
function testForDriveFiles() {
  testPromise(driveFileSystemPromise.then(function(volume) {
    return launchWithEntries(volume.entries);
  }));
}

/**
 * Tests the directory handler feature with entries on the local volume.
 */
function testForLocalDirectories() {
  testPromise(localDirSystemPromise.then(function(volume) {
    return launchWithEntries(volume.entries);
  }));
}

/**
 * Tests the directory handler feature with entries on the local volume.
 */
function testForDriveDirectories() {
  testPromise(driveDirSystemPromise.then(function(volume) {
    return launchWithEntries(volume.entries);
  }));
}

/**
 * Tests the file and directory handler with entries both on the local and on
 * the drive volumes.
 */
function testForMixedFilesAndDirectories() {
  testPromise(
      Promise.all([localFileSystemPromise, driveFileSystemPromise,
                   localDirSystemPromise, driveDirSystemPromise]).then(
          function(args) {
            return launchWithEntries(args[0].entries.concat(args[1].entries)
                .concat(args[2].entries).concat(args[3].entries));
          }));
}

// Run the tests.
chrome.test.runTests([
  testForLocalFiles,
  testForDriveFiles,
  testForLocalDirectories,
  testForDriveDirectories,
  testForMixedFilesAndDirectories,
]);
