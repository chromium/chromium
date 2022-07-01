// Copyright 2022 The Chromium Authors. All rights reserved.
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
    chrome.fileSystem.requestFileSystem(
        {volumeId: volume.volumeId, writable: true}, fs => {
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

chrome.test.runTests([
  async function getDlpMetadata() {
    const testEntries = await getFileEntries(
        'testing',
        ['blocked_file.txt', 'unrestricted_file.txt', 'untracked_file.txt']);
    chrome.test.assertEq(3, testEntries.length);
    chrome.fileManagerPrivate.getDlpMetadata(
        testEntries, chrome.test.callbackPass(dlpMetadata => {
          chrome.test.assertEq(
              [
                {isDlpRestricted: true, sourceUrl: 'https://example1.com'},
                {isDlpRestricted: false, sourceUrl: 'https://example2.com'},
                {isDlpRestricted: false, sourceUrl: ''}
              ],
              dlpMetadata);
        }))
  },
  async function getDlpMetadata_Error() {
    // Get the file.
    const [file] = await getFileEntries('testing', ['blocked_file.txt']);
    // Delete the file. Even though 'blocked_file.txt' is restricted by DLP,
    // once it doesn't exist anymore an empty DlpMetadata object should be
    // returned.
    await new Promise((resolve, reject) => file.remove(resolve, reject));
    chrome.fileManagerPrivate.getDlpMetadata(
        [file], chrome.test.callbackPass(dlpMetadata => {
          chrome.test.assertEq(
              [{isDlpRestricted: false, sourceUrl: ''}], dlpMetadata);
        }))
  },
  async function getDlpMetadata_Empty() {
    chrome.fileManagerPrivate.getDlpMetadata(
        [], chrome.test.callbackPass(dlpMetadata => {
          chrome.test.assertEq(0, dlpMetadata.length);
        }))
  }
]);
