// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Requests metadata for all loaded Volumes.
 * @return {!Promise<chrome.fileManagerPrivate.VolumeMetadata>} Volume metadata
 */
function getVolumeMetadataList() {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.getVolumeMetadataList(resolve);
  });
}

/**
 * Gets the root DirectoryEntry for the Volume with ID |volumeId|.
 *
 * |volumeId| can be arbitrary, so the caller can specify the key with which
 * the Volume's root should be associated.
 *
 * @param {string} volumeId The Volume to get the root for.
 * @param {string} volumeKey The key with which to associate the Volume
 * @returns {!Promise<Object<string, DirectoryEntry>>}
 */
function getVolumeRoot(volumeId, volumeKey) {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.getVolumeRoot(
        {volumeId: volumeId}, function(rootDirectoryEntry) {
          if (chrome.runtime.lastError) {
            reject(chrome.runtime.lastError.message);
          } else {
            resolve({'volumeKey': volumeKey, 'root': rootDirectoryEntry});
          }
        });
  });
}

/**
 * Requests access to the Downloads and Test volumes, returning a Promise that
 * holds an array of associations between a volumeKey (eg. "downloads") and
 * that Volume's root DirectoryEntry.
 * @returns {!Promise<Array<Object<string, DirectoryEntry>>>}
 */
function getTestVolumeRoots() {
  return getVolumeMetadataList().then(function(volumes) {
    const testVolumes = volumes.filter(
        volume =>
            (volume.volumeId.startsWith('downloads:') ||
             volume.volumeId.startsWith('testing:')));
    return Promise.all(testVolumes.map(function(volume) {
      return getVolumeRoot(volume.volumeId, volume.volumeId.split(':')[0]);
    }));
  });
}

// Run the tests.
getTestVolumeRoots().then(function(testVolumeRoots) {
  // Convert the array of <string, DirectoryEntry> into a map.
  const volumeRootsByVolumeKey =
      testVolumeRoots.reduce(function(map, volumeAssociation) {
        map[volumeAssociation.volumeKey] = volumeAssociation.root;
        return map;
      }, {});

  chrome.test.runTests([
    // Test that addFileWatch succeeds on a watchable volume ("downloads").
    async function testAddFileWatchToWatchableVolume() {
      await chrome.fileManagerPrivate.addFileWatch(
          volumeRootsByVolumeKey['downloads'], () => {
            chrome.test.assertNoLastError();
            chrome.test.succeed();
          });
    },

    // Test that addFileWatch fails on a non-watchable volume ("testing").
    async function testAddFileWatchToNonWatchableVolume() {
      await chrome.fileManagerPrivate.addFileWatch(
          volumeRootsByVolumeKey['testing'], () => {
            chrome.test.assertLastError('Volume is not watchable');
            chrome.test.succeed();
          });
    }
  ]);
});
