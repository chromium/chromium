// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Gets an external file entry from a path.
 * @param {string} volumeType volume type for entry.
 * @param {string} path path of entry.
 * @return {!Promise<Entry>} specified entry.
 */
function getFileEntry(volumeType, path) {
  return new Promise(resolve => {
    chrome.fileManagerPrivate.getVolumeMetadataList(list => {
      const volume = list.find(v => v.volumeType === volumeType);
      chrome.fileSystem.requestFileSystem({volumeId: volume.volumeId}, fs => {
        fs.root.getFile(path, {}, entry => {
          chrome.fileManagerPrivate.resolveIsolatedEntries(
              [entry], externalEntries => {
                resolve(externalEntries[0]);
              });
        });
      });
    });
  });
}

// Run the tests.
chrome.test.runTests([
  function testGetHoldingSpaceFails() {
    chrome.fileManagerPrivate.getHoldingSpaceState(
        chrome.test.callbackFail('Not enabled'));
  },

  function testAddSingleEntry() {
    getFileEntry('testing', 'test_dir/test_file.txt')
        .then(chrome.test.callbackPass(entry => {
          chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
              [entry], true, chrome.test.callbackFail('Not enabled'));
        }));
  },
]);
