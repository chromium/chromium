// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, getVolumeInfo, promisifyWithLastError, remoteProvider, unmount} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

/**
 * Listens for a single onMountCompleted event.
 * @returns {!Promise<!chrome.fileManagerPrivate.MountCompletedEvent>}
 */
async function nextOnMountCompletedEvent() {
  return new Promise(resolve => {
    const handler = (e) => {
      resolve(e);
      chrome.fileManagerPrivate.onMountCompleted.removeListener(handler);
    };
    chrome.fileManagerPrivate.onMountCompleted.addListener(handler);
  });
}

async function main() {
  await navigator.serviceWorker.ready;

  const firstFileSystemId = 'vanilla';
  const secondFileSystemId = 'ice-cream';

  await Promise.all([
    chrome.fileSystemProvider.mount(
        {fileSystemId: firstFileSystemId, displayName: 'vanilla.zip'}),
    chrome.fileSystemProvider.mount(
        {fileSystemId: secondFileSystemId, displayName: 'ice-cream.zip'}),
  ]);

  chrome.test.runTests([
    // Tests the fileSystemProvider.unmount(). Verifies if the unmount event
    // is emitted by VolumeManager.
    async function unmountSuccessful() {
      const unmountEventPromise = nextOnMountCompletedEvent();

      await unmount(firstFileSystemId);

      const e = await unmountEventPromise;
      chrome.test.assertEq('unmount', e.eventType);
      chrome.test.assertEq('success', e.status);
      // For extension based providers, provider id is the same as
      // extension id.
      chrome.test.assertEq(chrome.runtime.id, e.volumeMetadata.providerId);
      chrome.test.assertEq(firstFileSystemId, e.volumeMetadata.fileSystemId);
      chrome.test.succeed();
    },

    // Tests the fileSystemProvider.unmount() with a wrong id. Verifies that
    // it fails with a correct error code.
    async function unmountWrongId() {
      await remoteProvider.resetState();
      const error = await catchError(unmount('wrong-fs-id'));

      chrome.test.assertTrue(!!error, 'Unmount should have failed.');
      chrome.test.assertEq('NOT_FOUND', error.message);
      chrome.test.succeed();
    },

    // Tests if fileManagerPrivate.removeMount() for provided file systems emits
    // the onMountRequested() event with correct arguments.
    async function requestUnmountSuccess() {
      await remoteProvider.resetState();
      const volumeInfo = await getVolumeInfo(secondFileSystemId);
      await promisifyWithLastError(
          chrome.fileManagerPrivate.removeMount, volumeInfo.volumeId);

      const {fileSystemId} =
          await remoteProvider.waitForEvent('onUnmountRequested');
      chrome.test.assertEq(secondFileSystemId, fileSystemId);
      chrome.test.succeed();
    },

    // End-to-end test with a failure. Invokes fileSystemProvider.removeMount()
    // on a provided file system, and verifies (1) if the onMountRequested()
    // event is called with correct arguments, and (2) if calling onError(),
    // results in an unmount event fired from the VolumeManager instance.
    async function requestUnmountError() {
      await remoteProvider.resetState();
      // Next call to unmount should return error.
      await remoteProvider.setConfig(
          'onUnmountRequestedError',
          chrome.fileSystemProvider.ProviderError.IN_USE,
      );
      const unmountEventPromise = nextOnMountCompletedEvent();

      const volumeInfo = await getVolumeInfo(secondFileSystemId);
      await promisifyWithLastError(
          chrome.fileManagerPrivate.removeMount, volumeInfo.volumeId);

      const {fileSystemId} =
          await remoteProvider.waitForEvent('onUnmountRequested');
      chrome.test.assertEq(secondFileSystemId, fileSystemId);
      const e = await unmountEventPromise;
      chrome.test.assertEq('unmount', e.eventType);
      chrome.test.assertEq('unknown_error', e.status);
      // For extension based providers, provider id is the same as
      // extension id.
      chrome.test.assertEq(chrome.runtime.id, e.volumeMetadata.providerId);
      chrome.test.assertEq(secondFileSystemId, e.volumeMetadata.fileSystemId);
      chrome.test.succeed();
    }
  ]);
}

main();
