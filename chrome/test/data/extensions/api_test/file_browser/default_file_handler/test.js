// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Runs a test to verify that the file handler task picked for
 * a '.tiff' file matches against the filename extension in
 * the manifest and is selected as the default handler as a result.
 * It also checks that matching as a generic file handler fails to
 * set the handler as default.
 */

/**
 * File for which the file browser handlers will be executed by the extension.
 * @type {string}
 */
const kTestPaths = ['test_dir/test_file.xul', 'test_dir/test_file.tiff'];

// Starts the test extension.
function run() {
  /**
   * Test cases after the file path has been resolved to FileEntry. Each
   * resolved test case contains the resolved FileEntry object.
   *
   * @type {!Array<!FileEntry>}
   */
  let resolvedEntries = [];
  /**
   * Whether the test extension has done its job. When done is set |onError|
   * calls will be ignored.
   * @type {boolean}
   */
  let done = false;
  /*
   * Function called when an error is encountered. It sends test failure
   * notification.
   *
   * @param {string} errorMessage The error message to be send.
   */
  function onError(errorMessage) {
    if (done)
      return;

    chrome.test.notifyFail(errorMessage);
    // there should be at most one notifyFail call.
    done = true;
  }

  /**
   * Callback to chrome.fileManagerPrivate.getFileTasks.
   * It remembers the returned task id and entry. When tasks for all test cases
   * are found, they are executed.
   *
   * @param {!Entry} entry File entry for which getFileTasks was called.
   * @param {!chrome.fileManagerPrivate.ResultingTasks} resultingTasks List of
   *     found task objects.
   */
  function onGotTasks(entry, resultingTasks) {
    if (!resultingTasks || !resultingTasks.tasks) {
      onError('Failed getting tasks for ' + entry.fullPath);
      return;
    }
    const tasks = resultingTasks.tasks;

    if (tasks.length != 1) {
      onError('Got invalid number of tasks for "' + entry.fullPath + '": ' +
              tasks.length);
    }

    const {appId, taskType, actionId} = tasks[0].descriptor;
    const encodedTaskId = `${appId}|${taskType}|${actionId}`;
    // Check this test extension which explicitly declares itself as a handler
    // for the filename extension '.tiff' will match and set itself as default
    // for a path ending in '.tiff'
    const tiffex = /.*\.tiff/;
    if (tiffex.test(entry.fullPath)) {
      if (encodedTaskId != 'pkplfbidichfdicaijlchgnapepdginl|app|image') {
        onError(`Got invalid task ${encodedTaskId} for "${entry.fullPath}"`);
      }
      if (!tasks[0].isDefault) {
        onError(`Task "${encodedTaskId}" should be default for "${
            entry.fullPath}"`);
      }
    }
    else {  // Matched file extension that's not '.tiff'
      if (encodedTaskId != 'pkplfbidichfdicaijlchgnapepdginl|app|any') {
        onError(`Got invalid task ${encodedTaskId} for "${entry.fullPath}"`);
      }
      if (tasks[0].isDefault) {
        onError(`Task "${encodedTaskId}" is default for "${entry.fullPath}"`);
      }
    }
    if (resolvedEntries.length == kTestPaths.length) {
      chrome.test.succeed();
    }
  }

  /**
   * Success callback for getFile operation. Remembers resolved test case, and
   * when all the test cases have been resolved, gets file tasks for each of
   * them.
   *
   * @param {FileEntry} isolatedEntry The file entry for the test case.
   */
  function onGotEntry(isolatedEntry) {
    chrome.fileManagerPrivate.resolveIsolatedEntries(
        [isolatedEntry],
        function(externalEntries) {
          resolvedEntries.push(externalEntries[0]);
          if (resolvedEntries.length == kTestPaths.length) {
            resolvedEntries.forEach(function(entry) {
              chrome.fileManagerPrivate.getFileTasks(
                  [entry], [''], onGotTasks.bind(null, entry));
            });
          }
        });
  }

  /**
   * Called when the test mount point has been determined. It starts resolving
   * test cases (i.e. getting file entries for the test file paths).
   *
   * @param {DOMFileSystem} fileSystem The testing volume.
   * @param {string} volumeType Type of the volume.
   */
  function onGotFileSystem(fileSystem, volumeType) {
    var isOnDrive = volumeType == 'drive';
    kTestPaths.forEach(function(filePath) {
      fileSystem.root.getFile(
          (isOnDrive ? 'root/' : '') + filePath, {},
          onGotEntry.bind(null),
          onError.bind(null, 'Unable to get file: ' + filePath));
    });
  }

  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    const volume = volumeMetadataList.find((volume) => {
                   return volume.volumeType == 'testing'; });
    if (!volume) {
      onError('No volumes available, which could be used for testing.');
      return;
    }
    chrome.fileSystem.requestFileSystem(
        {volumeId: volume.volumeId},
        function(fileSystem) {
          if (!fileSystem) {
            onError('Failed to acquire the testing volume.');
            return;
          }
          onGotFileSystem(fileSystem, volume.volumeType);
        });
  });
}

// Start the testing.
run();
