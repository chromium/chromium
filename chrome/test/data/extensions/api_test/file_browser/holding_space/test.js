// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callbackPass = chrome.test.callbackPass;

/**
 * Gets an external file entry from specified path.
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

/**
 * Wrapper around <code>getFileEntry()</code> that resolves multiple paths.
 * @param {string}  volumeType
 * @param {Array<string>} paths
 * @return {!Promise<Array<Entry>>}
 */
function getFileEntries(volumeType, paths) {
  return Promise.all(paths.map(path => getFileEntry(volumeType, path)));
}

/**
 * Converts test file entry indices in |testEntries| into their associated URL.
 * @param {Array<number>} indices
 * @return {Array<string>}
 */
function testItemIndicesToUrls(indices) {
  return indices.map(index => testEntries[index]).map(entry => entry.toURL());
}

/**
 * List of entries used in tests.
 * @type {Array<Entry>}
 */
let testEntries = [];

// Run the tests.
chrome.test.runTests([
  function testGetTestEntries() {
    getFileEntries('testing', [
      'test_dir/test_file.txt', 'test_audio.mp3', 'test_image.jpg'
    ]).then(callbackPass(entries => {
      testEntries = entries;
    }));
  },

  function testEmptyHoldingSpace() {
    chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
      chrome.test.assertEq({itemUrls: []}, state);
    }));
  },

  function testAddSingleEntry() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[0]], true, callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            const expectedUrls = testItemIndicesToUrls([0]);
            chrome.test.assertEq({itemUrls: expectedUrls}, state);
          }));
        }));
  },

  function testAddTwoEntries() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[1], testEntries[2]], true, callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            const expectedUrls = testItemIndicesToUrls([0, 1, 2]);
            chrome.test.assertEq({itemUrls: expectedUrls}, state);
          }));
        }));
  },

  function testRemoveTwoEntries() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[0], testEntries[2]], false, callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            const expectedUrls = testItemIndicesToUrls([1]);
            chrome.test.assertEq({itemUrls: expectedUrls}, state);
          }));
        }));
  },

  function testAddPreviouslyAddedItem() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[0], testEntries[1]], true, callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            const expectedUrls = testItemIndicesToUrls([1, 0]);
            chrome.test.assertEq({itemUrls: expectedUrls}, state);
          }));
        }));
  },

  function testRemoveSingleItem() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[1]], false, callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            const expectedUrls = testItemIndicesToUrls([0]);
            chrome.test.assertEq({itemUrls: expectedUrls}, state);
          }));
        }));
  },

  function testRemoveAllItemsItem() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[0], testEntries[1], testEntries[2]], false,
        callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            chrome.test.assertEq({itemUrls: []}, state);
          }));
        }));
  },

  function testAddAllItemsItem() {
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        [testEntries[0], testEntries[1], testEntries[2]], true,
        callbackPass(() => {
          chrome.fileManagerPrivate.getHoldingSpaceState(callbackPass(state => {
            const expectedUrls = testItemIndicesToUrls([0, 1, 2]);
            chrome.test.assertEq({itemUrls: expectedUrls}, state);
          }));
        }));
  },
]);
