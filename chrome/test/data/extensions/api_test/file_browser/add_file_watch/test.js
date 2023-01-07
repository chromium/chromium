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
    chrome.fileManagerPrivate.getVolumeRoot({volumeId}, function(root) {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError.message);
      } else {
        resolve({volumeId, volumeKey, root});
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

/** Async wrapper for chrome.fileManager.addFileWatch() */
async function addFileWatch(...args) {
  return new Promise(resolve => {
    chrome.fileManagerPrivate.addFileWatch(...args, resolve);
  });
}

/** Async wrapper for chrome.fileManager.removeFileWatch() */
async function removeFileWatch(...args) {
  return new Promise(resolve => {
    chrome.fileManagerPrivate.removeFileWatch(...args, resolve);
  });
}

/** Async wrapper for chrome.fileManager.removeMount() */
async function removeMount(...args) {
  return new Promise(resolve => {
    chrome.fileManagerPrivate.removeMount(...args, resolve);
  });
}

// Run the tests.
getTestVolumeRoots().then(function(testVolumeRoots) {
  // Convert the array of <string, DirectoryEntry> into a map.
  const volumesByVolumeKey =
      testVolumeRoots.reduce(function(map, volumeAssociation) {
        map[volumeAssociation.volumeKey] = volumeAssociation;
        return map;
      }, {});

  chrome.test.runTests([
    // Test that addFileWatch succeeds on a watchable volume ("downloads").
    async function testAddFileWatchToWatchableVolume() {
      const downloads = volumesByVolumeKey['downloads'];
      await addFileWatch(downloads.root);
      chrome.test.assertNoLastError();
      await removeFileWatch(downloads.root);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    },

    // Test that addFileWatch fails on a non-watchable volume ("testing").
    async function testAddFileWatchToNonWatchableVolume() {
      const testing = volumesByVolumeKey['testing'];
      await addFileWatch(testing.root);
      chrome.test.assertLastError('Volume is not watchable');
      await removeFileWatch(testing.root);
      // chrome.test.assertNoLastError();
      chrome.test.succeed();
    },

    // Test that removeFileWatcher doesn't fail after unmounting the volume.
    async function testRemoveFileWatcherAfterUnmounting() {
      const volume = volumesByVolumeKey['testing'];
      await addFileWatch(volume.root);
      chrome.test.assertLastError('Volume is not watchable');

      // Unmount the testing volume.
      await removeMount(volume.volumeId);
      chrome.test.assertNoLastError();

      await removeFileWatch(volume.root);
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    },
  ]);
});
