// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, mountTestFileSystem, promisifyWithLastError, Queue, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';
// For shared constants.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/**
 *
 * @param {!Entry} entry
 * @returns {!Promise<!Entry>}
 */
async function toExternalEntry(entry) {
  const entries = await promisifyWithLastError(
      chrome.fileManagerPrivate.resolveIsolatedEntries, [entry]);
  if (!entries || entries.length == 0) {
    throw new Error('Failed to convert th entry to external entry.');
  }
  return entries[0];
}

/**
 * Get watchers of the test filesystem.
 * @return {!Promise<!Array<!chrome.fileSystemProvider.Watcher>>}
 */
async function getWatchers() {
  const fsInfo = await promisifyWithLastError(
      chrome.fileSystemProvider.get, TestFileSystemProvider.FILESYSTEM_ID);
  return fsInfo.watchers;
}

async function main() {
  await navigator.serviceWorker.ready;

  const fileSystem = await mountTestFileSystem({supportsNotifyTag: true});

  const testDir = 'notify-directory';

  const directoryChangeEvents = new Queue();
  chrome.fileManagerPrivate.onDirectoryChanged.addListener(
      e => directoryChangeEvents.push(e));

  /** @returns {!Promise<!Object>} */
  const getNextDirectoryChangeEvent = async () => {
    return directoryChangeEvents.pop();
  };

  await remoteProvider.addFiles({
    [`/${testDir}`]: {
      metadata: {
        isDirectory: true,
        name: testDir,
        size: 0,
        modificationTime: new Date(2014, 4, 28, 10, 39, 15),
      },
    },
  });

  /**
   * @returns {!Promise<!Entry>}
   */
  const getTestDirEntry = async () => {
    const dirEntry =
        await fileSystem.getDirectoryEntry(testDir, {create: false});
    return toExternalEntry(dirEntry);
  };

  chrome.test.runTests([
    // Add a watcher, and then notify that the entry has changed
    // from the service worker.
    async function notifySuccessFromServiceWorker() {
      const testTag = 'event-1';
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();
      chrome.test.assertTrue(await promisifyWithLastError(
          chrome.fileManagerPrivate.addFileWatch, dirEntry));

      // Notify will be called from the service worker.
      await remoteProvider.triggerNotify(
          dirEntry.fullPath, /*recursive=*/ false, testTag);

      const changeEvent = await getNextDirectoryChangeEvent();
      chrome.test.assertEq('changed', changeEvent.eventType);
      chrome.test.assertEq(dirEntry.toURL(), changeEvent.entry.toURL());
      // Confirm that the tag is updated.
      const watchers = await getWatchers();
      chrome.test.assertEq(1, watchers.length);
      chrome.test.assertEq(testTag, watchers[0].lastTag);
      chrome.test.succeed();
    },

    // Notify that the entry has changed from the foreground
    // page.
    async function notifySuccessFromForeground() {
      // Tag must be different each event.
      const testTag = 'event-2';
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();
      // Re-using the watcher added in the previous test.

      await promisifyWithLastError(chrome.fileSystemProvider.notify, {
        fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
        observedPath: dirEntry.fullPath,
        recursive: false,
        changeType: 'CHANGED',
        tag: testTag,
      });

      const changeEvent = await getNextDirectoryChangeEvent();
      chrome.test.assertEq('changed', changeEvent.eventType);
      chrome.test.assertEq(dirEntry.toURL(), changeEvent.entry.toURL());
      // Confirm that the tag is updated.
      const watchers = await getWatchers();
      chrome.test.assertEq(1, watchers.length);
      chrome.test.assertEq(testTag, watchers[0].lastTag);
      chrome.test.succeed();
    },

    // Notify with the same tag should be an error.
    async function notifyErrorSameTag() {
      const testTag = 'event-2';
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();

      const error = await catchError(
          promisifyWithLastError(chrome.fileSystemProvider.notify, {
            fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
            observedPath: dirEntry.fullPath,
            recursive: false,
            changeType: 'CHANGED',
            tag: testTag,
          }));

      chrome.test.assertTrue(!!error, 'Expected notify to fail.');
      chrome.test.assertEq('INVALID_OPERATION', error.message);
      chrome.test.assertEq(0, directoryChangeEvents.size());
      chrome.test.succeed();
    },

    // Passing an empty tag is invalid when the file system
    // supports the tag.
    async function notifyErrorEmptyTag() {
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();

      const error = await catchError(
          promisifyWithLastError(chrome.fileSystemProvider.notify, {
            fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
            observedPath: dirEntry.fullPath,
            recursive: false,
            changeType: 'CHANGED',
            tag: '',
          }));

      chrome.test.assertTrue(!!error, 'Expected notify to fail.');
      chrome.test.assertEq('INVALID_OPERATION', error.message);
      chrome.test.assertEq(0, directoryChangeEvents.size());
      chrome.test.succeed();
    },

    // Passing no tag is invalid when the file system supports
    // the tag.
    async function notifyErrorNoTag() {
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();

      const error = await catchError(
          promisifyWithLastError(chrome.fileSystemProvider.notify, {
            fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
            observedPath: dirEntry.fullPath,
            recursive: false,
            changeType: 'CHANGED',
            // No tag specified.
          }));

      chrome.test.assertTrue(!!error, 'Expected notify to fail.');
      chrome.test.assertEq('INVALID_OPERATION', error.message);
      chrome.test.assertEq(0, directoryChangeEvents.size());
      chrome.test.succeed();
    },

    // Notifying for the watched entry but in a wrong mode
    // (recursive, while the watcher is not recursive) should
    // fail.
    async function notifyErrorDifferentModeTag() {
      const testTag = 'event-3';
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();

      const error = await catchError(
          promisifyWithLastError(chrome.fileSystemProvider.notify, {
            fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
            observedPath: dirEntry.fullPath,
            recursive: true,
            changeType: 'CHANGED',
            tag: testTag,
          }));

      chrome.test.assertTrue(!!error, 'Expected notify to fail.');
      chrome.test.assertEq('NOT_FOUND', error.message);
      chrome.test.assertEq(0, directoryChangeEvents.size());
      chrome.test.succeed();
    },

    // Notify about the watched entry being removed. That should result in the
    // watcher being removed.
    async function notifyDeleted() {
      const testTag = 'event-4';
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();
      // Re-using the watcher added in the previous test.

      await promisifyWithLastError(chrome.fileSystemProvider.notify, {
        fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
        observedPath: dirEntry.fullPath,
        recursive: false,
        changeType: 'DELETED',
        tag: testTag,
      });

      const changeEvent = await getNextDirectoryChangeEvent();
      chrome.test.assertEq('changed', changeEvent.eventType);
      chrome.test.assertEq(dirEntry.toURL(), changeEvent.entry.toURL());
      // Confirm that the watcher is removed.
      const watchers = await getWatchers();
      chrome.test.assertEq(0, watchers.length);
      chrome.test.succeed();
    },

    // Notify about an entry which is not watched. That should result in an
    // error.
    async function notifyNotWatched() {
      const testTag = 'event-5';
      directoryChangeEvents.clear();
      let dirEntry = await getTestDirEntry();

      const error = await catchError(
          promisifyWithLastError(chrome.fileSystemProvider.notify, {
            fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
            observedPath: dirEntry.fullPath,
            recursive: true,
            changeType: 'CHANGED',
            tag: testTag,
          }));

      chrome.test.assertTrue(!!error, 'Expected notify to fail.');
      chrome.test.assertEq('NOT_FOUND', error.message);
      chrome.test.assertEq(0, directoryChangeEvents.size());
      chrome.test.succeed();
    },
  ]);
}

main();
