// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Runs test to verify that file browser tasks can successfully be executed.
 * The test does the following:
 * - Open external filesystem.
 * - Get the test file system. The root is determined by probing volumes with
 *   whitelisted ids.
 * - Get files 'test_dir/test_file.xul' and 'test_dir/test_file.tiff'
 *   on the test mount point.
 *   Chrome part of the test should ensure that these actually exist.
 * - For each of the files do following:
 *   - Get registered file tasks.
 *   - Verify there is exactly one registered task (Chrome part of the test
 *     should ensure this: it should launch an extension that has exactly one
 *     handler for each of files).
 *   - Execute the task.
 *
 * If there is a failure in any of these steps, the extension reports it back to
 * Chrome, otherwise it does nothing. The responsibility to report test result
 * is given to file task handler.
 */

/**
 * Files for which the file browser handlers will be executed by the extension.
 * @type {Array<string>}
 */
var kTestPaths = ['test_dir/test_file.xul', 'test_dir/test_file.tiff'];

// Starts the test extension.
function run() {
  /**
   * Test cases after the file path has been resolved to FileEntry. Each
   * resolved test case contains the resolved FileEntry object.
   *
   * @type {!Array<!FileEntry>}
   */
  var resolvedEntries = [];

  /**
   * List of tasks found for a testCase. Each object contains the found task id
   * and entry for which the task should be executed.
   *
   * @type {!Array<!Object<string, !Entry>>}
   */
  var foundTasks = [];

  /**
   * Whether the test extension has done its job. When done is set |onError|
   * calls will be ignored.
   * @type {boolean}
   */
  var done = false;

  /**
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
   * Callback to chrome.fileManagerPrivate.executeTask. Verifies the function
   * succeeded.
   *
   * @param {!Entry} entry The entry of file for which the handler was executed.
   * @param {boolean} success Whether the function succeeded.
   */
  function onExecuteTask(entry, success) {
    if (!success)
      onError('Failed to execute task for ' + entry.fullPath);
  }

  /**
   * Callback to chrome.fileManagerPrivate.getFileTasks.
   * It checks that the returned task is not the default, sets it as the default
   * and calls getFileTasks again.
   *
   * @param {string} entry File entry for which getFileTasks was called.
   * @param {!chrome.fileManagerPrivate.ResultingTasks} resultingTasks List of
   *     found task objects.
   */

  function onGotNonDefaultTasks(entry, resultingTasks) {
    if (!resultingTasks || !resultingTasks.tasks) {
      onError('Failed getting tasks for ' + entry.fullPath);
      return;
    }
    const tasks = resultingTasks.tasks;
    if (tasks.length != 1) {
      onError('Got invalid number of tasks for "' + entry.fullPath + '": ' +
              tasks.length);
    }
    // Task could be default from an explicit file extension match
    // but if matched on MIME type we need to set the task as default
    chrome.fileManagerPrivate.setDefaultTask(
      tasks[0].descriptor, [entry], [], function() {
          if (chrome.runtime.lastError) {
            const {appId, taskType, actionId} = tasks[0].descriptor;
            onError(`Failed to set a task to default: ${appId}|${taskType}|${
                actionId}`);
            return;
          }
          chrome.fileManagerPrivate.getFileTasks(
              [entry], [''], onGotTasks.bind(null, entry));
        });
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
    const taskId = `${appId}|${taskType}|${actionId}`;
    if (!tasks[0].isDefault) {
      onError(`Task "${taskId}" is not default for "${entry.fullPath}"`);
    }

    foundTasks.push({descriptor: tasks[0].descriptor, entry: entry});

    if (foundTasks.length == kTestPaths.length) {
      foundTasks.forEach(function(task) {
        chrome.fileManagerPrivate.executeTask(task.descriptor, [task.entry],
            onExecuteTask.bind(null, task.entry));
      });
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
    // TODO(mtomasz): Remove this hack after migrating chrome.fileManagerPrivate
    // API to isolated context.
    chrome.fileManagerPrivate.resolveIsolatedEntries(
        [isolatedEntry],
        function(externalEntries) {
          resolvedEntries.push(externalEntries[0]);
          if (resolvedEntries.length == kTestPaths.length) {
            resolvedEntries.forEach(function(entry) {
              chrome.fileManagerPrivate.getFileTasks(
                  [entry], [''], onGotNonDefaultTasks.bind(null, entry));
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
    // Try to acquire the first volume which is either TESTING or DRIVE type.
    var possibleVolumeTypes = ['testing', 'drive'];
    var sortedVolumeMetadataList = volumeMetadataList.filter(function(volume) {
      return possibleVolumeTypes.indexOf(volume.volumeType) != -1;
    }).sort(function(volumeA, volumeB) {
      return possibleVolumeTypes.indexOf(volumeA.volumeType) -
             possibleVolumeTypes.indexOf(volumeB.volumeType);
    });
    if (sortedVolumeMetadataList.length == 0) {
      onError('No volumes available, which could be used for testing.');
      return;
    }
    chrome.fileSystem.requestFileSystem(
        {volumeId: sortedVolumeMetadataList[0].volumeId},
        function(fileSystem) {
          if (!fileSystem) {
            onError('Failed to acquire the testing volume.');
            return;
          }
          onGotFileSystem(fileSystem, sortedVolumeMetadataList[0].volumeType);
        });
  });
}

// Start the testing.
run();
