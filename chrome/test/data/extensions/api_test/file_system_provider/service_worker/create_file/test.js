// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

/**
 * @type {Object}
 * @const
 */
const TESTING_FILE = Object.freeze({
  isDirectory: false,
  name: 'kitty',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

/**
 * @type {Object}
 * @const
 */
const TESTING_NEW_FILE = Object.freeze({
  isDirectory: false,
  name: 'puppy',
  size: 0,
  modificationTime: new Date(2014, 4, 28, 10, 39, 15)
});

async function main() {
  await navigator.serviceWorker.ready;
  await remoteProvider.addFiles({
    ['/' + TESTING_FILE.name]: {
      metadata: TESTING_FILE,
      contents: '',
    },
  });

  const {fileSystem} = await mountTestFileSystem();

  /**
   * @param {string} path
   * @param {{create: boolean, exclusive: boolean}} options
   * @returns {!Promise<!FileEntry>}
   */
  const getFileEntry = (path, options) => {
    return new Promise(
        (resolve, reject) =>
            fileSystem.root.getFile(path, options, resolve, reject));
  };

  chrome.test.runTests([
    // Create a file which doesn't exist. Should succeed.
    async function createFileSuccessSimple() {
      try {
        const fileEntry = await getFileEntry(
            TESTING_NEW_FILE.name, {create: true, exclusive: false});

        chrome.test.assertEq(TESTING_NEW_FILE.name, fileEntry.name);
        chrome.test.assertFalse(fileEntry.isDirectory);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Create a file which exists, non-exclusively. Should succeed.
    async function createFileOrOpenSuccess() {
      try {
        const fileEntry = await getFileEntry(
            TESTING_FILE.name, {create: true, exclusive: false});

        chrome.test.assertEq(TESTING_FILE.name, fileEntry.name);
        chrome.test.assertFalse(fileEntry.isDirectory);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Create a file which exists, exclusively. Should fail.
    async function createFileExistsError() {
      try {
        await getFileEntry(TESTING_FILE.name, {create: true, exclusive: true});

        chrome.test.fail('Created a file, but should fail.');
      } catch (e) {
        chrome.test.assertEq('InvalidModificationError', e.name);
        chrome.test.succeed();
      }
    }
  ]);
}

main();
