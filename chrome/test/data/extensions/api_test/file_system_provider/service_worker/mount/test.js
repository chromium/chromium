// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {catchError, getFsInfoById, getVolumeInfo, promisifyWithLastError, remoteProvider} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

/**
 * @param {{
 *    fileSystemId: string,
 *    displayName: string,
 *    openedFilesLimit: (number|undefined),
 *    writable: (boolean|undefined),
 *    supportsNotifyTag: (boolean|undefined),
 *  }} options
 */
async function mount({
  openedFilesLimit,
  fileSystemId,
  displayName,
  writable,
  supportsNotifyTag
}) {
  await promisifyWithLastError(chrome.fileSystemProvider.mount, {
    fileSystemId,
    displayName,
    openedFilesLimit,
    writable,
    supportsNotifyTag,
  });
}

async function main() {
  await navigator.serviceWorker.ready;

  chrome.test.runTests([
    // Tests whether mounting succeeds, when a non-empty name is provided.
    async function goodDisplayName() {
      await mount(
          {fileSystemId: 'file-system-id', displayName: 'file-system-name'});

      chrome.test.succeed();
    },

    // Verifies that mounting fails, when an empty string is provided as a name.
    async function emptyDisplayName() {
      const e = await catchError(
          mount({fileSystemId: 'file-system-id-2', displayName: ''}));

      chrome.test.assertTrue(!!e, 'Mount expected to fail.');
      chrome.test.assertEq('INVALID_OPERATION', e.message);
      chrome.test.succeed();
    },

    // Verifies that mounting fails, when an empty string is provided as an Id
    async function emptyFileSystemId() {
      const e = await catchError(
          mount({fileSystemId: '', displayName: 'File System Name'}));

      chrome.test.assertTrue(!!e, 'Mount expected to fail.');
      chrome.test.assertEq('INVALID_OPERATION', e.message);
      chrome.test.succeed();
    },

    // Verifies that mounting succeeds, when a positive limit for opened files
    // is provided.
    async function goodOpenedFilesLimit() {
      await mount({
        fileSystemId: 'file-system-id-3',
        displayName: 'File System Name',
        openedFilesLimit: 10
      });

      chrome.test.succeed();
    },

    // Verifies that mounting succeeds, when limit for number of opened files is
    // set to 0. It means no limit.
    async function goodOpenedFilesLimit() {
      await mount({
        fileSystemId: 'file-system-id-4',
        displayName: 'File System Name',
        openedFilesLimit: 0
      });

      chrome.test.succeed();
    },

    // Verifies that mounting fails, when a negative limit for opened files is
    // provided.
    async function illegalOpenedFilesLimit() {
      const e = await catchError(mount({
        fileSystemId: 'file-system-id-5',
        displayName: 'File System Name',
        openedFilesLimit: -1
      }));

      chrome.test.assertTrue(!!e, 'Mount expected to fail.');
      chrome.test.assertEq('INVALID_OPERATION', e.message);
      chrome.test.succeed();
    },

    // End-to-end test. Mounts a volume using fileSystemProvider.mount(), then
    // checks if the mounted volume is added to VolumeManager, by querying
    // fileManagerPrivate.getVolumeMetadataList().
    async function successfulMount() {
      const fileSystemId = 'caramel-candy';
      await mount({
        fileSystemId,
        displayName: 'caramel-candy.zip',
      });

      const volumeInfo = await getVolumeInfo(fileSystemId);
      chrome.test.assertTrue(volumeInfo.isReadOnly);
      chrome.test.succeed();
    },

    // Checks whether mounting a file system in writable mode ends up on filling
    // out the volume info properly.
    async function successfulWritableMount() {
      const fileSystemId = 'caramel-fudges';
      await mount({
        fileSystemId,
        displayName: 'caramel-fudges.zip',
        writable: true,
      });

      const volumeInfo = await getVolumeInfo(fileSystemId);
      chrome.test.assertFalse(volumeInfo.isReadOnly);
      chrome.test.succeed();
    },

    // Checks that providing writable=false is processed as read-only.
    async function writableFalse() {
      const fileSystemId = 'read-only';
      await mount({
        fileSystemId,
        displayName: 'read-only.zip',
        writable: false,
      });

      const volumeInfo = await getVolumeInfo(fileSystemId);
      chrome.test.assertTrue(volumeInfo.isReadOnly);
      chrome.test.succeed();
    },

    // Checks that providing supportsNotifyTag=false|true persists as requested.
    async function supportsNotifyTag() {
      // Test with supportsNotifyTag=true
      const taggedFilesystemId = 'tagged-fs';
      await mount({
        fileSystemId: taggedFilesystemId,
        displayName: 'tagged-fs.zip',
        supportsNotifyTag: true,
      });

      const fsInfoTagged = await getFsInfoById(taggedFilesystemId);
      chrome.test.assertTrue(!!fsInfoTagged);
      chrome.test.assertEq(fsInfoTagged.supportsNotifyTag, true);

      // Test with supportsNotifyTag=false
      const nonTaggedFilesystemId = 'non-tagged-fs';
      await mount({
        fileSystemId: nonTaggedFilesystemId,
        displayName: 'non-tagged-fs.zip',
        supportsNotifyTag: false,
      });

      const fsInfoNonTagged = await getFsInfoById(nonTaggedFilesystemId);
      chrome.test.assertTrue(!!fsInfoNonTagged);
      chrome.test.assertEq(fsInfoNonTagged.supportsNotifyTag, false);
      chrome.test.succeed();
    },

    // Checks is limit for mounted file systems per profile works correctly.
    // Tries to create more than allowed number of file systems. All of the
    // mount requests should succeed, except the last one which should fail with
    // a security error.
    async function stressMountTest() {
      // Mount file systems up to the limit.
      const alreadyMountedFileSystems = 8;  // By previous tests.
      const maxFileSystems = 16;
      for (let i = alreadyMountedFileSystems; i < maxFileSystems; i++) {
        await mount({
          fileSystemId: `${i}-stress-test`,
          displayName: `File System #${i}`,
        });
      }

      // One over the limit should fail.
      const e = await catchError(mount({
        fileSystemId: 'over-the-limit-fs-id',
        displayName: 'Over The Limit File System'
      }));

      chrome.test.assertTrue(!!e, 'Mount expected to fail.');
      chrome.test.assertEq('TOO_MANY_OPENED', e.message);
      chrome.test.succeed();
    },

    // Tests if fileManagerPrivate.addProvidedFileSystem() emits the
    // onMountRequested() event.
    async function requestMountSuccess() {
      await remoteProvider.resetState();
      let providers =
          await promisifyWithLastError(chrome.fileManagerPrivate.getProviders);
      // Filter out native providers.
      providers = providers.filter(
          provider =>
              provider.providerId.length == 0 || provider.providerId[0] != '@');
      chrome.test.assertEq(providers.length, 1);
      chrome.test.assertEq(chrome.runtime.id, providers[0].providerId);
      chrome.test.assertEq(
          chrome.runtime.getManifest().name, providers[0].name);
      chrome.test.assertFalse(providers[0].configurable);
      chrome.test.assertFalse(providers[0].watchable);
      chrome.test.assertFalse(providers[0].multipleMounts);
      chrome.test.assertEq('network', providers[0].source);

      await promisifyWithLastError(
          chrome.fileManagerPrivate.addProvidedFileSystem, chrome.runtime.id);

      await remoteProvider.waitForEvent('onMountRequested');
      chrome.test.succeed();
    }
  ]);
}

main();
