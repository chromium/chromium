// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * Helper function that checks if the gotten entry list has entries with
 * wanted paths.
 */
function assertHasEntries(wantPathList, gotEntryList) {
  chrome.test.assertEq(
      wantPathList.length, gotEntryList.length,
      `Expected ${wantPathList.length}, found ${gotEntryList.length}`);
  for (let i = 0; i < wantPathList.length; ++i) {
    const wantPath = wantPathList[i];
    const gotEntry = gotEntryList[i];
    chrome.test.assertEq(
        wantPath, gotEntry.fullPath,
        `entry[${i}]: ${wantPath} != ${gotEntry.fullPath}`);
  }
}

/**
 * Helper function for getting the root directory for downloads.
 */
async function getDownloads() {
  return new Promise((resolve, reject) => {
    chrome.fileManagerPrivate.getVolumeMetadataList((volumeMetadaList) => {
      const downloads =
          volumeMetadaList.filter((v) => v.volumeId.startsWith('downloads:'))
      if (downloads.length !== 1) {
        reject(`Expected 1 downloads directory found ${downloads.length}`);
      } else {
        chrome.fileSystem.requestFileSystem(
            {
              volumeId: downloads[0].volumeId,
              writable: !downloads[0].isReadOnly
            },
            (fileSystem) => {
              resolve(fileSystem.root);
            });
      }
    });
  });
}

chrome.test.runTests([
  // Test the old style request without rootDir parameter.
  function testSearchWithoutRootDir() {
    chrome.fileManagerPrivate.searchFiles(
        {
          query: 'foo',
          types: 'ALL',
          maxResults: 10,
        },
        chrome.test.callbackPass((entryList) => {
          assertHasEntries(['/foo.jpg', '/images/foo.jpg'], entryList);
        }));
  },

  // Test the new style with explicit rootDir parameter set to the root.
  async function testSearchWithRootDirAtRoot() {
    const downloads = await getDownloads();
    chrome.fileManagerPrivate.searchFiles(
        {
          rootDir: downloads,
          query: 'foo',
          types: 'ALL',
          maxResults: 10,
        },
        chrome.test.callbackPass((entryList) => {
          assertHasEntries(['/foo.jpg', '/images/foo.jpg'], entryList);
        }));
  },

  // Test the new style with explicit rootDir set to a subdirectory.
  async function testSearchWithRootDirAtImages() {
    const downloads = await getDownloads();
    downloads.getDirectory('images', {create: false}, (dir) => {
      chrome.fileManagerPrivate.searchFiles(
          {
            rootDir: dir,
            query: 'foo',
            types: 'ALL',
            maxResults: 10,
          },
          chrome.test.callbackPass((entryList) => {
            assertHasEntries(['/images/foo.jpg'], entryList);
          }));
    });
  },
]);
