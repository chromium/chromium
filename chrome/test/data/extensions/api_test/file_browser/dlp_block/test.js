// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Gets the metadata of a volume having a specified type.
 * @param {string} volumeType volume type for entry.
 * @return {!Promise<chrome.fileManagerPrivate.VolumeMetadata>} Volume metadata.
 */
async function getVolumeMetadataByType(volumeType) {
  return new Promise(
      (resolve,
       reject) => {chrome.fileManagerPrivate.getVolumeMetadataList(list => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve(list.find(v => v.volumeType === volumeType));
      })});
}

/**
 * Gets the file system of specific volume type.
 * @param {string} volumeType volume type.
 * @return {!Promise<chrome.fileManagerPrivate.FileSystem>} Volume metadata.
 */
async function getFileSystem(volumeType) {
  const volume = await getVolumeMetadataByType(volumeType);
  return new Promise((resolve, reject) => {
    chrome.fileSystem.requestFileSystem({volumeId: volume.volumeId}, fs => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError.message);
        return;
      }
      resolve(fs);
    });
  });
}

/**
 * Gets an external file entry from a specified path.
 * @param {string} volumeType volume type for entry.
 * @param {string} path path of entry.
 * @return {!Promise<Entry>} specified entry.
 */
async function getFileEntry(volumeType, path) {
  const fs = await getFileSystem(volumeType);
  return new Promise(resolve => {
    fs.root.getFile(path, {}, entry => {
      chrome.fileManagerPrivate.resolveIsolatedEntries(
          [entry], externalEntries => {
            resolve(externalEntries[0]);
          });
    });
  });
}

/**
 * Wrapper around getFileEntry() that resolves multiple paths.
 * @param {string}  volumeType
 * @param {Array<string>} paths
 * @return {!Promise<Array<Entry>>}
 */
async function getFileEntries(volumeType, paths) {
  return Promise.all(paths.map(path => getFileEntry(volumeType, path)));
}

/**
 * Get a directory entry from a specified path.
 * @param {string} volumeType volume type for entry.
 * @param {string} path path of entry.
 * @return {!DirectoryEntry} specified entry.
 */
async function getDirectoryEntry(volumeType, path) {
  const fs = await getFileSystem(volumeType);
  return new Promise(resolve => {
    fs.root.getDirectory(path, {}, entry => {
      resolve(entry);
    });
  });
}

chrome.test.getConfig(async (config) => {
  chrome.test.runTests([async function getDisallowedTransfes() {
    testEntries = await getFileEntries('testing', ['dlp_test_file.txt']);
    chrome.test.assertEq(1, testEntries.length);

    const destinationDirectory = await getDirectoryEntry('drive', 'subdir');

    await chrome.fileManagerPrivate.getDisallowedTransfers(
        testEntries, destinationDirectory, /*isMove=*/ true,
        (disallowed_entries) => {
          chrome.test.assertEq(1, disallowed_entries.length);
          chrome.test.succeed();
        });
  }]);
});
