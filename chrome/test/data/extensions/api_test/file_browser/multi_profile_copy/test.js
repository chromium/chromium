// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var kSecondaryDriveMountPointName = "drive-fileBrowserApiTestProfile2";

/**
 * Returns a callback that works as an error handler in file system API
 * functions and invokes |callback| with specified message string.
 *
 * @param {function(string)} callback Wrapped callback function object.
 * @param {string} message Error message.
 * @return {function(DOMError)} Resulting callback function object.
 */
function fileErrorCallback(callback, message) {
  return function(error){
    callback(message + ": " + error.name);
  };
}

/**
 * Copies an entry using chrome.fileManagerPrivate.startCopy().
 *
 * @param {Entry} fromRoot Root entry of the copy source file system.
 * @param {string} fromPath Relative path from fromRoot of the source entry.
 * @param {Entry} toRoot Root entry of the copy destination file system.
 * @param {string} toPath Relative path from toRoot of the target directory.
 * @param {string} newName Name of the new copied entry.
 * @param {function()} successCallback Callback invoked when copy succeed.
 * @param {function(string)} errorCallback Callback invoked in error case.
 */
function fileCopy(fromRoot, fromPath, toRoot, toPath, newName,
                  successCallback, errorCallback) {
  fromRoot.getFile(fromPath, {create: false}, function(from) {
    toRoot.getDirectory(toPath, {create: false}, function(to) {
      var copyId = null;
      var onProgress = function(id, status) {
        if (id != copyId) {
          errorCallback('Unknown copy id.');
          return;
        }
        if (status.type == 'error') {
          chrome.fileManagerPrivate.onCopyProgress.removeListener(onProgress);
          errorCallback('Copy failed.');
          return;
        }
        if (status.type == 'success') {
          chrome.fileManagerPrivate.onCopyProgress.removeListener(onProgress);
          successCallback();
        }
      };
      chrome.fileManagerPrivate.onCopyProgress.addListener(onProgress);
      chrome.fileManagerPrivate.startCopy(
        from, to, newName, function(startCopyId) {
          if (chrome.runtime.lastError) {
            errorCallback('Error starting to copy.');
            return;
          }
          copyId = startCopyId;
        });
    }, fileErrorCallback(errorCallback, 'Error getting destination entry'));
  }, fileErrorCallback(errorCallback, 'Error getting source entry'));
}

/**
 * Verifies that a file exists on the specified location.
 *
 * @param {Entry} root Root entry of the file system.
 * @param {string} path Relative path of the file from the root entry,
 * @param {function()} successCallback Callback invoked when the file exists.
 * @param {function(string)} errorCallback Callback invoked in error case.
 */
function verifyFileExists(root, path, successCallback, errorCallback) {
  root.getFile(path, {create: false},
               successCallback,
               fileErrorCallback(errorCallback, path + ' does not exist.'));
}

/**
 * Collects all tests that should be run for the test volume.
 *
 * @param {Entry} firstRoot Root entry of the first volume.
 * @param {Entry} secondRoot Root entry of the second volume.
 * @return {Array<function()>} The list of tests that should be run.
 */
function collectTests(firstRoot, secondRoot) {
  var testsToRun = [];

  testsToRun.push(function crossProfileNormalFileCopyTest() {
    fileCopy(secondRoot, 'root/test_dir/test_file.tiff',
             firstRoot, 'root/',
             'newname.tiff',
             verifyFileExists.bind(null, firstRoot, 'root/newname.tiff',
                                   chrome.test.succeed, chrome.test.fail),
             chrome.test.fail);
  });

  testsToRun.push(function crossProfileHostedDocumentCopyTest() {
    fileCopy(secondRoot, 'root/test_dir/hosted_doc.gdoc',
             firstRoot, 'root/',
             'newname.gdoc',
             verifyFileExists.bind(null, firstRoot, 'root/newname.gdoc',
                                   chrome.test.succeed, chrome.test.fail),
             chrome.test.fail);
  });

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
    var driveVolumes = volumeMetadataList.filter(function(volume) {
      return volume.volumeType == 'drive';
    });

    if (driveVolumes.length != 1) {
      callback(null, 'Unexpected number of Drive volumes.');
      return;
    }

    chrome.fileSystem.requestFileSystem(
        {
          volumeId: driveVolumes[0].volumeId,
          writable: true
        },
        function(primaryFileSystem) {
          if (!primaryFileSystem) {
            callback(null, 'Failed to acquire the testing volume.');
            return;
          }

          // Resolving the isolated entry is necessary in order to fetch URL
          // of an entry from a different profile.
          chrome.fileManagerPrivate.resolveIsolatedEntries(
              [primaryFileSystem.root],
              function(entries) {
                var url = entries[0].toURL().replace(
                    /[^\/]*\/?$/, kSecondaryDriveMountPointName);
                chrome.fileManagerPrivate.grantAccess([url], function() {
                  webkitResolveLocalFileSystemURL(url, function(entry) {
                    if (!entry) {
                      callback(
                          null,
                          'Failed to acquire secondary profile\'s volume.');
                      return;
                    }

                    callback(collectTests(entries[0], entry), 'Success.');
                  }, () => {
                    callback(null, 'Failed to resolve ' + url);
                  });
                });
              });
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
