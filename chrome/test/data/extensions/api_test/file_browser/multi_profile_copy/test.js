// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const kSecondaryDriveMountPointName = 'drive-fileBrowserApiTestProfile2';

/**
 * @param {function(...?)} fn
 * @param {...?} args
 * @returns {!Promise<?>}
 */
async function promisifyWithLastError(fn, ...args) {
  return new Promise((resolve, reject) => {
    fn(...args, (result) => {
      const error = chrome.runtime.lastError;
      if (error) {
        reject(new Error(error.message));
      } else {
        resolve(result);
      }
    });
  });
}
/**
 * Get a file entry from the root of the mounted filesystem.
 *
 * @param {DirectoryEntry} root
 * @param {string} path
 * @param {{create: (boolean|undefined), exclusive: (boolean|undefined)}}
 *     options
 * @returns {!Promise<!Entry>}
 */
async function getFileEntry(root, path, options) {
  return new Promise(
      (resolve, reject) => root.getFile(path, options, resolve, reject));
}

/**
 * Get a directory entry from the root of the mounted filesystem.
 *
 * @param {DirectoryEntry} root
 * @param {string} path
 * @param {{create: (boolean|undefined), exclusive: (boolean|undefined)}}
 *  options
 * @returns {!Promise<!DirectoryEntry>}
 */
async function getDirectoryEntry(root, path, options) {
  return new Promise(
      (resolve, reject) => root.getDirectory(path, options, resolve, reject));
}


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
 * @param {!Entry} fromRoot Root entry of the copy source file system.
 * @param {string} fromPath Relative path from fromRoot of the source entry.
 * @param {!Entry} toRoot Root entry of the copy destination file system.
 * @param {string} toPath Relative path from toRoot of the target directory.
 * @param {function()} successCallback Callback invoked when copy succeed.
 * @param {function(string)} errorCallback Callback invoked in error case.
 */
async function fileCopy(
    fromRoot, fromPath, toRoot, toPath, successCallback, errorCallback) {
  const from = await getFileEntry(fromRoot, fromPath, {create: false});
  const to = await getDirectoryEntry(toRoot, toPath, {create: false});
  let copyId = null;
  const onProgress = function(event) {
    const id = event.taskId;
    if (id !== copyId) {
      // The first progress update comes before the `copyId` is assigned.
      // So here we just ignore the first update.
      return;
    }
    switch (event.state) {
      case chrome.fileManagerPrivate.IoTaskState.ERROR:
      case chrome.fileManagerPrivate.IoTaskState.CANCELLED:
        chrome.fileManagerPrivate.onIOTaskProgressStatus.removeListener(
            onProgress);
        errorCallback('Copy failed.');
        return;
      case chrome.fileManagerPrivate.IoTaskState.SUCCESS:
        chrome.fileManagerPrivate.onIOTaskProgressStatus.removeListener(
            onProgress);
        successCallback();
    }
  };

  chrome.fileManagerPrivate.onIOTaskProgressStatus.addListener(onProgress);

  copyId = await promisifyWithLastError(
      chrome.fileManagerPrivate.startIOTask,
      chrome.fileManagerPrivate.IoTaskType.COPY, [from],
      {destinationFolder: to});
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
 * @param {!Entry} firstRoot Root entry of the first volume.
 * @param {!Entry} secondRoot Root entry of the second volume.
 * @return {!Array<function()>} The list of tests that should be run.
 */
function collectTests(firstRoot, secondRoot) {
  const testsToRun = [];

  testsToRun.push(function crossProfileNormalFileCopyTest() {
    fileCopy(
        secondRoot, 'root/test_dir/test_file.tiff', firstRoot, 'root/',
        verifyFileExists.bind(
            null, firstRoot, 'root/test_file.tiff', chrome.test.succeed,
            chrome.test.fail),
        chrome.test.fail);
  });

  testsToRun.push(function crossProfileHostedDocumentCopyTest() {
    fileCopy(
        secondRoot, 'root/test_dir/hosted_doc.gdoc', firstRoot, 'root/',
        verifyFileExists.bind(
            null, firstRoot, 'root/hosted_doc.gdoc', chrome.test.succeed,
            chrome.test.fail),
        chrome.test.fail);
  });

  return testsToRun;
}

async function main() {
  const volumeMetadataList = await promisifyWithLastError(
      chrome.fileManagerPrivate.getVolumeMetadataList);
  const driveVolumes =
      volumeMetadataList.filter((volume) => volume.volumeType == 'drive');

  if (driveVolumes.length !== 1) {
    chrome.test.fail('Unexpected number of Drive volumes.');
    return;
  }

  const primaryFileSystem = await promisifyWithLastError(
      chrome.fileSystem.requestFileSystem,
      {volumeId: driveVolumes[0].volumeId, writable: true});

  if (!primaryFileSystem) {
    chrome.test.fail('Failed to acquire the testing volume.');
    return;
  }

  // Resolving the isolated entry is necessary in order to fetch URL of an entry
  // from a different profile.
  const entries = await promisifyWithLastError(
      chrome.fileManagerPrivate.resolveIsolatedEntries,
      [primaryFileSystem.root]);
  const secondaryUrl =
      entries[0].toURL().replace(/[^\/]*\/?$/, kSecondaryDriveMountPointName);
  await promisifyWithLastError(
      chrome.fileManagerPrivate.grantAccess, [secondaryUrl]);
  const secondaryEntry = await promisifyWithLastError(
      webkitResolveLocalFileSystemURL, secondaryUrl);
  if (!secondaryEntry) {
    chrome.test.fail('Failed to acquire secondary profile\'s volume.');
    return;
  }

  const tests = collectTests(entries[0], secondaryEntry);
  chrome.test.runTests(tests);
}

main();
