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
      `Expected ${wantPathList.length} entries, found ${gotEntryList.length}`);
  chrome.test.assertTrue(
      gotEntryList.every(e => wantPathList.includes(e.fullPath)),
      `${wantPathList.sort()} != ${gotEntryList.map(e => e.fullPath)}`);
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

  async function testSearchWithDateFilter() {
    const anyTimeEntries = await new Promise((resolve) => {
      chrome.fileManagerPrivate.searchFiles(
          {
            query: 'bar',
            types: 'ALL',
            maxResults: 10,
            // undefined timestamp finds everything older than Jan 01, 1970.
          },
          (entryList) => {
            resolve(entryList);
          });
    });
    assertHasEntries(
        ['/bar_01012020.jpg', '/bar_15012020.jpg'], anyTimeEntries);

    // NOTE: Some filesystems use 1s resolution for modified time. Thus we add
    // 1s rather than 1ms when setting date higher than known modified time.
    const delta = 1000;

    const jan15Entries = await new Promise((resolve) => {
      chrome.fileManagerPrivate.searchFiles(
          {
            query: 'bar',
            types: 'ALL',
            maxResults: 10,
            modifiedTimestamp: 1579089600000 - delta,  // Jan 15 2020, noon - 1s
          },
          (entryList) => {
            resolve(entryList);
          });
    });
    assertHasEntries(['/bar_15012020.jpg'], jan15Entries);
    const jan01Entries = await new Promise((resolve) => {
      chrome.fileManagerPrivate.searchFiles(
          {
            query: 'bar',
            types: 'ALL',
            maxResults: 10,
            modifiedTimestamp: 1577880000000 - delta,  // Jan 01 2020, noon - 1s
          },
          (entryList) => {
            resolve(entryList);
          });
    });
    assertHasEntries(
        ['/bar_01012020.jpg', '/bar_15012020.jpg'], jan01Entries);
    const noEntries = await new Promise((resolve) => {
      chrome.fileManagerPrivate.searchFiles(
          {
            query: 'bar',
            types: 'ALL',
            maxResults: 10,
            modifiedTimestamp: 1579089600000 + delta,  // Jan 15 2020, noon + 1s
          },
          (entryList) => {
            resolve(entryList);
          });
    });
    assertHasEntries([], noEntries);
    chrome.test.succeed();
  },
]);
