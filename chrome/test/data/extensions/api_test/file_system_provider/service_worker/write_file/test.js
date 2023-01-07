// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {mountTestFileSystem, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/**
 * Initial contents of testing files.
 * @type {string}
 * @const
 */
export const TESTING_TEXT_TO_WRITE = 'Vanilla ice creams are the best.';

/**
 * @type {string}
 * @const
 */
export const TESTING_NEW_FILE_NAME = 'perfume.txt';

/**
 * @type {string}
 * @const
 */
export const TESTING_TIRAMISU_FILE_NAME = 'tiramisu.txt';

/**
 * @param {!FileEntry} fileEntry
 * @returns {!Promise<!FileWriter>}
 */
async function createWriter(fileEntry) {
  return new Promise(
      (resolve, reject) => fileEntry.createWriter(resolve, reject));
}

/**
 * @param {!FileWriter} writer
 * @param {string} text
 * @returns {!Promise<void>}
 */
async function writeTextToFile(writer, text) {
  const blob = new Blob([text], {type: 'text/plain'});
  return new Promise((resolve, reject) => {
    writer.onwriteend = e => {
      // Note that onwriteend() is called even if an error
      // happened.
      if (!writer.error) {
        resolve();
      }
    };
    writer.onerror = e => reject(writer.error);
    writer.onabort = e => reject(writer.error);
    writer.write(blob);
  });
}

async function main() {
  await navigator.serviceWorker.ready;
  await remoteProvider.addFiles({
    ['/' + TESTING_TIRAMISU_FILE_NAME]: {
      metadata: {
        isDirectory: false,
        name: TESTING_TIRAMISU_FILE_NAME,
        size: TestFileSystemProvider.INITIAL_TEXT.length,
        modificationTime: new Date(2014, 1, 24, 6, 35, 11),
      },
      contents: TestFileSystemProvider.INITIAL_TEXT,
    },
  });
  const fileSystem = await mountTestFileSystem();

  chrome.test.runTests([
    // Write contents to a non-existing file. It should succeed.
    async function writeNewFileSuccess() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TESTING_NEW_FILE_NAME, {create: true, exclusive: true});

        await writeTextToFile(
            await createWriter(fileEntry), TESTING_TEXT_TO_WRITE);

        const newContents =
            await remoteProvider.getFileContents('/' + TESTING_NEW_FILE_NAME);
        chrome.test.assertEq(TESTING_TEXT_TO_WRITE, newContents);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Overwrite contents in an existing file. It should succeed.
    async function overwriteFileSuccess() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TESTING_TIRAMISU_FILE_NAME, {create: true, exclusive: false});

        await writeTextToFile(
            await createWriter(fileEntry), TESTING_TEXT_TO_WRITE);

        const newContents = await remoteProvider.getFileContents(
            '/' + TESTING_TIRAMISU_FILE_NAME);
        chrome.test.assertEq(TESTING_TEXT_TO_WRITE, newContents);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Append contents to an existing file. It should succeed.
    async function appendFileSuccess() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TESTING_TIRAMISU_FILE_NAME, {create: false, exclusive: false});

        const fileWriter = await createWriter(fileEntry);
        fileWriter.seek(TESTING_TEXT_TO_WRITE.length);
        await writeTextToFile(fileWriter, TESTING_TEXT_TO_WRITE);

        const newContents = await remoteProvider.getFileContents(
            '/' + TESTING_TIRAMISU_FILE_NAME);
        chrome.test.assertEq(
            TESTING_TEXT_TO_WRITE + TESTING_TEXT_TO_WRITE, newContents);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Replace contents in an existing file. It should succeed.
    async function replaceFileSuccess() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TESTING_TIRAMISU_FILE_NAME, {create: false, exclusive: false});

        const fileWriter = await createWriter(fileEntry);
        fileWriter.seek(TESTING_TEXT_TO_WRITE.indexOf('creams'));
        await writeTextToFile(fileWriter, 'skates');

        const expectedContents =
            TESTING_TEXT_TO_WRITE.replace('creams', 'skates') +
            TESTING_TEXT_TO_WRITE;
        const newContents = await remoteProvider.getFileContents(
            '/' + TESTING_TIRAMISU_FILE_NAME);
        chrome.test.assertEq(expectedContents, newContents);
        chrome.test.succeed();
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Write bytes to a broken file. This should result in an error.
    async function writeBrokenFileError() {
      try {
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_FAIL,
            {create: false, exclusive: false});
        const fileWriter = await createWriter(fileEntry);
        try {
          await writeTextToFile(fileWriter, 'A lot of flowers.');
          chrome.test.fail('Unexpectedly succeeded to write to a broken file.');
        } catch (e) {
          chrome.test.assertEq('InvalidStateError', e.name);
          chrome.test.succeed();
        }
      } catch (e) {
        chrome.test.fail(e);
      }
    },

    // Abort writing to a valid file with a registered abort handler. Should
    // result in a gracefully terminated writing operation.
    async function abortWritingSuccess() {
      await remoteProvider.resetState();
      let writePromise;
      try {
        // A write to this file will be stuck forever.
        const fileEntry = await fileSystem.getFileEntry(
            TestFileSystemProvider.FILE_BLOCKS_FOREVER,
            {create: false, exclusive: false});
        const fileWriter = await createWriter(fileEntry);

        // Start a write request, wait for it to reach the provider.
        writePromise = writeTextToFile(fileWriter, 'A lot of cherries.');
        await remoteProvider.waitForEvent('onWriteFileRequested');

        // Abort the operation after it's started.
        fileWriter.abort();
        await remoteProvider.waitForEvent('onAbortRequested');
      } catch (e) {
        chrome.test.fail(e);
      }

      try {
        await writePromise;
        chrome.test.fail('Unexpectedly finished writing, despite aborting.');
      } catch (e) {
        chrome.test.assertEq('AbortError', e.name);
        chrome.test.succeed();
      }
    }
  ]);
}

main();
