// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Helper function that retrieves volume ID of the test folder.
 * @return {Promise<string>} the promise that returns the volume ID
 */
function getTestVolumeId() {
  return new Promise((resolve, reject) => {
    chrome.fileManagerPrivate.getVolumeMetadataList(volumeMetadataList => {
      const testVolumes =
          volumeMetadataList.filter(volume => volume.volumeType === 'testing');
      if (testVolumes.length === 1) {
        resolve(testVolumes[0].volumeId);
      } else {
        reject(new Error(`Found ${testVolumes.length} test volumes`));
      }
    });
  });
}

chrome.test.runTests([
  // Checks that we can request read-only access to a file system.
  function testRequestNonexistingFileSystem() {
    const volumeId = 'no-such-volume-id'
    chrome.fileManagerPrivate.getVolumeRoot(
        {
          volumeId: volumeId,
        },
        chrome.test.callbackFail(`Volume with ID '${volumeId}' not found`));
  },

  function testExistingFileSystem() {
    getTestVolumeId().then(volumeId => {
      chrome.fileManagerPrivate.getVolumeRoot(
          {
            volumeId: volumeId,
            writable: false,
          },
          chrome.test.callbackPass((rootDir) => {
            chrome.test.assertTrue(rootDir.isDirectory);
            chrome.test.assertEq('/', rootDir.fullPath);
            chrome.test.assertTrue(rootDir.filesystem !== undefined &&
                rootDir.filesystem !== null);
          }));
    });
  }
]);
