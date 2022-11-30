// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Component extension that tests the extensions with fileBrowesrPrivate
 * permission are able to perform file system operations on external file
 * systems. The test can be run for three different external file system types:
 * local native, restricted local native and drive. Depending on the type,
 * a file system with specific volumeId is acquired. C++ side of the test
 * (external_file_system_extension_apitest.cc) should make sure the file systems
 * are available, and the volume IDs are correct.
 *
 * The test files on file systems should be created before running the test
 * extension. The test extension expects following hierarchy:
 *   (root) - test_dir - subdir
 *                         |
 *                          - empty_dir
 *                         |
 *                          - test_file.xul
 *                         |
 *                          - test_file.xul.foo
 *                         |
 *                          - test_file.tiff
 *                         |
 *                          - test_file.tiff.foo
 *
 * 'root' exists only for Drive.
 * (root/)test_dir/subdir/ will be used as destination dir for copy and move
 * operations.
 * (root/)test_dir/empty_dir/ should be empty and will stay empty until
 * the end of the test.
 * (root/)test_dir/test_file.xul will not change during the test.
 *
 * All files should initially have content: kInitialFileContent.
 */

var kInitialFileContent = 'This is some test content.';
var kWriteOffset = 26;
var kWriteData = ' Yay!';
var kFileContentAfterWrite = 'This is some test content. Yay!';
var kTruncateShortLength = 4;
var kFileContentAfterTruncateShort = 'This';
var kTruncateLongLength = 6;
var kFileContentAfterTruncateLong = 'This\0\0';

function assertEqAndRunCallback(expectedValue, value, errorMessage,
                                callback, callbackArg) {
  chrome.test.assertEq(expectedValue, value, errorMessage);
  callback(callbackArg);
}

/**
 * Helper methods for performing file system operations during tests.
 * Each of them will call |callback| on success, or fail the current test
 * otherwise.
 *
 * callback for |getDirectory| and |getFile| functions expects the gotten entry
 * as an argument. For Other methods, the callback argument should be ignored.
 */

// Gets the path for operations. The path is relative to the volume for
// local entries and relative to the "My Drive" root for Drive entries.
function getPath(relativePath, isOnDrive) {
  return (isOnDrive ? 'root/' : '') + relativePath;
}

// Gets the directory entry.
function getDirectory(
    volumeId, entry, path, shouldCreate, expectSuccess, callback) {
  var messagePrefix = shouldCreate ? 'Creating ' : 'Getting ';
  var message = messagePrefix + 'directory: \'' + path +'\'.';
  var isOnDrive = volumeId == 'drive:drive-user';

  entry.getDirectory(
      getPath(path, isOnDrive), {create: shouldCreate},
      assertEqAndRunCallback.bind(null, expectSuccess, true, message, callback),
      assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                  callback, null));
}

// Gets the file entry.
function getFile(volumeId, entry, path, shouldCreate, expectSuccess, callback) {
  var messagePrefix = shouldCreate ? 'Creating ' : 'Getting ';
  var message = messagePrefix + 'file: \'' + path +'\'.';
  var isOnDrive = volumeId == 'drive:drive-user';

  entry.getFile(
      getPath(path, isOnDrive), {create: shouldCreate},
      assertEqAndRunCallback.bind(null, expectSuccess, true, message, callback),
      assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                  callback, null));
}

// Reads file entry/path and verifies its content. The read operation
// should always succeed.
function readFileAndExpectContent(
    volumeId, entry, path, expectedContent, callback) {
  var message = 'Content of the file \'' + path + '\'.';
  getFile(volumeId, entry, path, false, true, function(entry) {
    var reader = new FileReader();
    reader.onload = function() {
      assertEqAndRunCallback(expectedContent, reader.result, message, callback);
    };
    reader.onerror = function(event) {
      chrome.test.fail('Failed to read: ' + reader.error.name);
    };
    entry.file(reader.readAsText.bind(reader),
               function(error) {
                 chrome.test.fail('Failed to get file: ' + error.name);
               });
  });
}

// Writes |content| to the file entry/path  with offest |offest|.
function writeFile(
    volumeId, entry, path, offset, content, expectSuccess, callback) {
  var message = 'Writing to file: \'' + path + '\'.';

  getFile(volumeId, entry, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      writer.onwrite = assertEqAndRunCallback.bind(null,
          expectSuccess, true, message, callback);
      writer.onerror = assertEqAndRunCallback.bind(null,
          expectSuccess, false, message, callback);

      writer.seek(offset);
      writer.write(new Blob([content], {'type': 'text/plain'}));
    },
    assertEqAndRunCallback.bind(null, expectSuccess, false,
        'Creating writer for \'' + path + '\'.', callback));
  });
}

// Starts and aborts write operation to entry/path.
function abortWriteFile(volumeId, entry, path, callback) {
  getFile(volumeId, entry, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      var aborted = false;
      var failed = false;

      writer.onwritestart = function() { writer.abort(); };
      writer.onwrite = function() { failed = true; };
      writer.onerror = function() { failed = true; };
      writer.onabort = function() { aborted = true; };

      writer.onwriteend = function() {
        chrome.test.assertTrue(aborted);
        chrome.test.assertFalse(failed);
        callback();
      }

      writer.write(new Blob(['xxxxx'], {'type': 'text/plain'}));
    }, function(error) {
      chrome.test.fail('Error creating writer: ' + error.name);
    });
  });
}

// Truncates file entry/path to length |length|.
function truncateFile(volumeId, entry, path, length, expectSuccess, callback) {
  var message = 'Truncating file: \'' + path + '\' to length ' + length + '.';
  getFile(volumeId, entry, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      writer.onwrite = assertEqAndRunCallback.bind(null,
          expectSuccess, true, message, callback);
      writer.onerror = assertEqAndRunCallback.bind(null,
          expectSuccess, false, message, callback);

      writer.truncate(length);
    },
    assertEqAndRunCallback.bind(null, expectSuccess, false,
        'Creating writer for \'' + path + '\'.', callback));
  });
}

// Starts and aborts truncate operation on entry/path.
function abortTruncateFile(volumeId, entry, path, callback) {
  getFile(volumeId, entry, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      var aborted = false;
      var failed = false;

      writer.onwritestart = function() { writer.abort(); };
      writer.onwrite = function() { failed = true; };
      writer.onerror = function() { failed = true; };
      writer.onabort = function() { aborted = true; };

      writer.onwriteend = function() {
        chrome.test.assertTrue(aborted);
        chrome.test.assertFalse(failed);
        callback();
      }

      writer.truncate(10);
    }, function(error) {
      chrome.test.fail('Error creating writer: ' + error.name);
    });
  });
}

// Copies file entry/path from to entry/to/newName.
function copyFile(volumeId, entry, from, to, newName, expectSuccess, callback) {
  var message = 'Copying \'' + from + '\' to \'' + to + '/' + newName + '\'.';

  getFile(volumeId, entry, from, false, true, function(sourceEntry) {
    getDirectory(volumeId, entry, to, false, true, function(targetDir) {
      sourceEntry.copyTo(targetDir, newName,
          assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                      callback),
          assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                      callback));
    });
  });
}

// Moves file entry/from to entry/to/newName.
function moveFile(volumeId, entry, from, to, newName, expectSuccess, callback) {
  var message = 'Moving \'' + from + '\' to \'' + to + '/' + newName + '\'.';

  getFile(volumeId, entry, from, false, true, function(sourceEntry) {
    getDirectory(volumeId, entry, to, false, true, function(targetDir) {
      sourceEntry.moveTo(targetDir, newName,
          assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                      callback),
          assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                      callback));
    });
  });
}

// Deletes file entry/path.
function deleteFile(volumeId, entry, path, expectSuccess, callback) {
  var message = 'Deleting file \'' + path + '\'.';

  getFile(volumeId, entry, path, false, true, function(entry) {
    entry.remove(
        assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                    callback),
        assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                    callback));
  });
}

// Deletes directory entry/path.
function deleteDirectory(volumeId, entry, path, expectSuccess, callback) {
  var message = 'Deleting directory \'' + path + '\'.';

  getDirectory(volumeId, entry, path, false, true, function(entry) {
    entry.remove(
        assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                    callback),
        assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                    callback));
  });
}

// Recursively deletes directory entry/path.
function deleteDirectoryRecursively(
    volumeId, entry, path, expectSuccess, callback) {
  var message = 'Recursively deleting directory \'' + path + '\'.';

  getDirectory(volumeId, entry, path, false, true, function(entry) {
    entry.removeRecursively(
        assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                    callback),
        assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                    callback));
  });
}

/**
 * Collects all tests that should be run for the test volume.
 *
 * @param {string} volumeId ID of the volume.
 * @param {DOMFileSystem} fileSystem File system of the volume.
 * @returns {Array<function()>} The list of tests that should be run.
 */
function collectTestsForVolumeId(volumeId, fileSystem) {
  console.log(volumeId);
  var isReadOnly = volumeId == 'testing:restricted';
  var isOnDrive = volumeId == 'drive:drive-user';

  var testsToRun = [];

  testsToRun.push(function getDirectoryTest() {
    getDirectory(volumeId, fileSystem.root, 'test_dir', false, true,
        chrome.test.succeed);
  });

  testsToRun.push(function createDirectoryTest() {
    // callback checks whether the new directory exists after create operation.
    // It should exists iff the file system is not read only.
    var callback = getDirectory.bind(null, volumeId, fileSystem.root,
        'new_test_dir', false, !isReadOnly, chrome.test.succeed);

    // Create operation should succeed only for non read-only file systems.
    getDirectory(volumeId, fileSystem.root, 'new_test_dir', true, !isReadOnly,
        callback);
  });

  testsToRun.push(function getFileTest() {
    getFile(volumeId, fileSystem.root, 'test_dir/test_file.xul', false, true,
            chrome.test.succeed);
  });

  testsToRun.push(function createFileTest() {
    // Checks whether the new file exists after create operation.
    // It should exists iff the file system is not read only.
    var callback = getFile.bind(null, volumeId, fileSystem.root,
        'test_dir/new_file', false, !isReadOnly, chrome.test.succeed);

    // Create operation should succeed only for non read-only file systems.
    getFile(volumeId, fileSystem.root, 'test_dir/new_file', true, !isReadOnly,
        callback);
  });

  testsToRun.push(function readFileTest() {
    readFileAndExpectContent(volumeId, fileSystem.root,
        'test_dir/test_file.xul', kInitialFileContent, chrome.test.succeed);
  });

  testsToRun.push(function writeFileTest() {
    var expectedFinalContent = isReadOnly ? kInitialFileContent :
                                            kFileContentAfterWrite;
    // Check file content after write operation. The content should not change
    // on read-only file system.
    var callback = readFileAndExpectContent.bind(null, volumeId,
        fileSystem.root, 'test_dir/test_file.tiff', expectedFinalContent,
        chrome.test.succeed);

    // Write should fail only on read-only file system.
    writeFile(volumeId, fileSystem.root, 'test_dir/test_file.tiff',
        kWriteOffset, kWriteData, !isReadOnly, callback);
  });

  testsToRun.push(function truncateFileShortTest() {
    var expectedFinalContent = isReadOnly ? kInitialFileContent :
                                            kFileContentAfterTruncateShort;
    // Check file content after truncate operation. The content should not
    // change on read-only file system.
    var callback = readFileAndExpectContent.bind(null, volumeId,
        fileSystem.root, 'test_dir/test_file.tiff', expectedFinalContent,
        chrome.test.succeed);

    // Truncate should fail only on read-only file system.
    truncateFile(volumeId, fileSystem.root, 'test_dir/test_file.tiff',
        kTruncateShortLength, !isReadOnly, callback);
  });

  testsToRun.push(function truncateFileLongTest() {
    var expectedFinalContent = isReadOnly ? kInitialFileContent :
                                            kFileContentAfterTruncateLong;
    // Check file content after truncate operation. The content should not
    // change on read-only file system.
    var callback = readFileAndExpectContent.bind(null, volumeId,
        fileSystem.root, 'test_dir/test_file.tiff', expectedFinalContent,
        chrome.test.succeed);

    // Truncate should fail only on read-only file system.
    truncateFile(volumeId, fileSystem.root, 'test_dir/test_file.tiff',
        kTruncateLongLength, !isReadOnly, callback);
  });

  // Skip abort tests for read-only file systems.
  if (!isReadOnly) {
    testsToRun.push(function abortWriteTest() {
      abortWriteFile(volumeId, fileSystem.root, 'test_dir/test_file.xul.foo',
                     chrome.test.succeed);
    });

    testsToRun.push(function abortTruncateTest() {
      abortTruncateFile(volumeId, fileSystem.root, 'test_dir/test_file.xul.foo',
                        chrome.test.succeed);
    });
  }

  testsToRun.push(function copyFileTest() {
    var verifyTarget = null;
    if (isReadOnly) {
      // If the file system is read-only, the target file should not exist after
      // copy operation.
      verifyTarget = getFile.bind(null, volumeId, fileSystem.root,
          'test_dir/subdir/copy', false, false, chrome.test.succeed);
    } else {
      // If the file system is not read-only, the target file should be created
      // during copy operation and its content should match the source file.
      verifyTarget = readFileAndExpectContent.bind(null, volumeId,
          fileSystem.root, 'test_dir/subdir/copy', kInitialFileContent,
          chrome.test.succeed);
    }

    // Verify the source file stil exists and its content hasn't changed.
    var verifySource = readFileAndExpectContent.bind(null, volumeId,
        fileSystem.root, 'test_dir/test_file.xul', kInitialFileContent,
        verifyTarget);

    // Copy file should fail on read-only file system.
    copyFile(volumeId, fileSystem.root, 'test_dir/test_file.xul',
        'test_dir/subdir', 'copy', !isReadOnly, chrome.test.succeed);
  });

  testsToRun.push(function moveFileTest() {
    var verifyTarget = null;
    if (isReadOnly) {
      // If the file system is read-only, the target file should not be created
      // during move.
      verifyTarget = getFile.bind(null, volumeId, fileSystem.root,
          'test_dir/subdir/move', false, false, chrome.test.succeed);
    } else {
      // If the file system is read-only, the target file should be created
      // during move and its content should match the source file.
      verifyTarget = readFileAndExpectContent.bind(null, volumeId,
          fileSystem.root, 'test_dir/subdir/move', kInitialFileContent,
          chrome.test.succeed);
    }

    // On read-only file system the source file should still exist. Otherwise
    // the source file should have been deleted during move operation.
    var verifySource = getFile.bind(null, volumeId, fileSystem.root,
        'test_dir/test_file.xul', false, isReadOnly, verifyTarget);

    // Copy file should fail on read-only file system.
    moveFile(volumeId, fileSystem.root, 'test_dir/test_file.xul',
        'test_dir/subdir', 'move', !isReadOnly, chrome.test.succeed);
  });

  testsToRun.push(function deleteFileTest() {
    // Verify that file exists after delete operation if and only if the file
    // system is read only.
    var callback = getFile.bind(null, volumeId, fileSystem.root,
        'test_dir/test_file.xul.foo', false, isReadOnly, chrome.test.succeed);

    // Delete operation should fail for read-only file systems.
    deleteFile(volumeId, fileSystem.root, 'test_dir/test_file.xul.foo',
        !isReadOnly, callback);
  });

  testsToRun.push(function deleteEmptyDirectoryTest() {
    // Verify that the directory exists after delete operation if and only if
    // the file system is read-only.
    var callback = getDirectory.bind(null, volumeId, fileSystem.root,
        'test_dir/empty_dir', false, isReadOnly, chrome.test.succeed);

    // Deleting empty directory should fail for read-only file systems, and
    // succeed otherwise.
    deleteDirectory(volumeId, fileSystem.root, 'test_dir/empty_dir',
        !isReadOnly, callback);
  });

  testsToRun.push(function deleteDirectoryTest() {
    // Verify that the directory still exists after the operation.
    var callback = getDirectory.bind(null, volumeId, fileSystem.root,
        'test_dir', false, true, chrome.test.succeed);

    // The directory should still contain some files, so non-recursive delete
    // should fail.
    deleteDirectory(volumeId, fileSystem.root, 'test_dir', false, callback);
  });

  // On drive, the directory was deleted in the previous test.
  if (!isOnDrive) {
    testsToRun.push(function deleteDirectoryRecursivelyTest() {
      // Verify that the directory exists after delete operation if and only if
      // the file system is read-only.
      var callback = getDirectory.bind(null, volumeId, fileSystem.root,
          'test_dir', false, isReadOnly, chrome.test.succeed);

      // Recursive delete dhouls fail only for read-only file system.
      deleteDirectoryRecursively(volumeId, fileSystem.root, 'test_dir',
          !isReadOnly, callback);
    });
  }

  return testsToRun;
}

/**
 * Initializes testParams.
 * Gets test volume and creates list of tests that should be run for it.
 *
 * @param {function(Array, string)} callback. Called with an array containing
 *     the list of the tests to run and an error message. On error list of tests
 *     to run will be null.
 */
function initTests(callback) {
  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    var possibleVolumeTypes = ['testing', 'drive'];

    var sortedVolumeMetadataList = volumeMetadataList.filter(function(volume) {
      return possibleVolumeTypes.indexOf(volume.volumeType) != -1;
    }).sort(function(volumeA, volumeB) {
      return possibleVolumeTypes.indexOf(volumeA.volumeType) -
             possibleVolumeTypes.indexOf(volumeB.volumeType);
    });

    if (sortedVolumeMetadataList.length == 0) {
      callback(null, 'No volumes available, which could be used for testing.');
      return;
    }

    chrome.fileSystem.requestFileSystem(
        {
          volumeId: sortedVolumeMetadataList[0].volumeId,
          writable: !sortedVolumeMetadataList[0].isReadOnly
        },
        function(fileSystem) {
          if (!fileSystem) {
            callback(null, 'Failed to acquire the testing volume.');
            return;
          }

          var testsToRun = collectTestsForVolumeId(
              sortedVolumeMetadataList[0].volumeId, fileSystem);
          callback(testsToRun, 'Success.');
        });
  });
}

// Trigger the tests.
initTests(function(testsToRun, errorMessage) {
  if (!testsToRun) {
    chrome.test.notifyFail('Failed to initialize tests: ' + errorMessage);
    return;
  }

  chrome.test.runTests(testsToRun);
});
