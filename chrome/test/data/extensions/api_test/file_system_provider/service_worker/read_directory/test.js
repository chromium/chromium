// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {catchError, mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

const TESTING_HELLO_DIR = Object.freeze({
  isDirectory: true,
  name: 'hello',
  modificationTime: new Date(2014, 1, 26, 8, 37, 13),
});

/**
 * @type {Object}
 * @const
 */
const TESTING_CANDIES_DIR = Object.freeze({
  isDirectory: true,
  name: 'candies',
  modificationTime: new Date(2014, 1, 26, 8, 37, 13),
});

/**
 * @type {Object}
 * @const
 */
const TESTING_TIRAMISU_FILE = Object.freeze({
  isDirectory: false,
  name: 'tiramisu.txt',
  modificationTime: new Date(2014, 1, 26, 8, 37, 13),
});

/**
 * Read all entries from a directory.
 *
 * @param {!DirectoryEntry} dirEntry
 * @returns {!Promise<!Array<!FileEntry>>}
 */
async function readAllEntries(dirEntry) {
  const allEntries = [];
  const reader = dirEntry.createReader();
  for (;;) {
    const entries = await new Promise(
        (resolve, reject) => reader.readEntries(resolve, reject));
    if (entries.length == 0) {
      break;
    }
    allEntries.push(...entries);
  }
  return allEntries;
}

async function main() {
  await navigator.serviceWorker.ready;
  const fileSystem = await mountTestFileSystem();
  await remoteProvider.addFiles({
    [`/${TESTING_HELLO_DIR.name}`]: {
      metadata: TESTING_HELLO_DIR,
    },
    [`/${TESTING_HELLO_DIR.name}/${TESTING_TIRAMISU_FILE.name}`]: {
      metadata: TESTING_TIRAMISU_FILE,
    },
    [`/${TESTING_HELLO_DIR.name}/${TESTING_CANDIES_DIR.name}`]: {
      metadata: TESTING_CANDIES_DIR,
    },
  });

  chrome.test.runTests([
    // Read contents of the /hello directory. This directory exists, so it
    // should succeed.
    async function readEntriesSuccess() {
      const dirEntry = await fileSystem.getDirectoryEntry(
          TESTING_HELLO_DIR.name, {create: false});
      const entries = await readAllEntries(dirEntry);

      chrome.test.assertEq(2, entries.length);
      chrome.test.assertTrue(entries[0].isFile);
      chrome.test.assertEq('tiramisu.txt', entries[0].name);
      chrome.test.assertEq('/hello/tiramisu.txt', entries[0].fullPath);
      chrome.test.assertTrue(entries[1].isDirectory);
      chrome.test.assertEq('candies', entries[1].name);
      chrome.test.assertEq('/hello/candies', entries[1].fullPath);
      chrome.test.succeed();
    },

    // Read contents of a directory which does not exist, what should return an
    // error.
    async function readEntriesError() {
      const error = await catchError(
          fileSystem.getDirectoryEntry('cranberries', {create: false}));

      chrome.test.assertTrue(
          !!error, 'Succeeded getting a non-existent directory entry.');
      chrome.test.assertEq('NotFoundError', error.name);
      chrome.test.succeed();
    }
  ]);
}

main();
