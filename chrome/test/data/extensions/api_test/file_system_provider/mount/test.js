// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Runs all of the test cases, one by one.
 */
chrome.test.runTests([
  // Tests whether mounting succeeds, when a non-empty name is provided.
  function goodDisplayName() {
    chrome.fileSystemProvider.mount(
        {fileSystemId: 'file-system-id', displayName: 'file-system-name'},
        chrome.test.callbackPass());
  },

  // Verifies that mounting fails, when an empty string is provided as a name.
  function emptyDisplayName() {
    chrome.fileSystemProvider.mount(
        {fileSystemId: 'file-system-id-2', displayName: ''},
        chrome.test.callbackFail('INVALID_OPERATION'));
  },

  // Verifies that mounting fails, when an empty string is provided as an Id
  function emptyFileSystemId() {
    chrome.fileSystemProvider.mount(
        {fileSystemId: '', displayName: 'File System Name'},
        chrome.test.callbackFail('INVALID_OPERATION'));
  },

  // Verifies that mounting succeeds, when a positive limit for opened files is
  // provided.
  function goodOpenedFilesLimit() {
    chrome.fileSystemProvider.mount({
      fileSystemId: 'file-system-id-3',
      displayName: 'File System Name',
      openedFilesLimit: 10
    }, chrome.test.callbackPass());
  },

  // Verifies that mounting succeeds, when limit for number of opened files is
  // set to 0. It means no limit.
  function goodOpenedFilesLimit() {
    chrome.fileSystemProvider.mount({
      fileSystemId: 'file-system-id-4',
      displayName: 'File System Name',
      openedFilesLimit: 0
    }, chrome.test.callbackPass());
  },

  // Verifies that mounting fails, when a negative limit for opened files is
  // provided.
  function illegalOpenedFilesLimit() {
    chrome.fileSystemProvider.mount({
      fileSystemId: 'file-system-id-5',
      displayName: 'File System Name',
      openedFilesLimit: -1
    }, chrome.test.callbackFail('INVALID_OPERATION'));
  },

  // End to end test. Mounts a volume using fileSystemProvider.mount(), then
  // checks if the mounted volume is added to VolumeManager, by querying
  // fileManagerPrivate.getVolumeMetadataList().
  function successfulMount() {
    var fileSystemId = 'caramel-candy';
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: fileSystemId,
          displayName: 'caramel-candy.zip',
        },
        chrome.test.callbackPass(function() {
          chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeList) {
            var volumeInfo;
            volumeList.forEach(function(inVolumeInfo) {
              // For extension based providers, provider id is the same as
              // extension id.
              if (inVolumeInfo.providerId === chrome.runtime.id &&
                  inVolumeInfo.fileSystemId === fileSystemId) {
                volumeInfo = inVolumeInfo;
              }
            });
            chrome.test.assertTrue(!!volumeInfo);
            chrome.test.assertTrue(volumeInfo.isReadOnly);
          });
        }));
  },

  // Checks whether mounting a file system in writable mode ends up on filling
  // out the volume info properly.
  function successfulWritableMount() {
    var fileSystemId = 'caramel-fudges';
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: fileSystemId,
          displayName: 'caramel-fudges.zip',
          writable: true
        },
        chrome.test.callbackPass(function() {
          chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeList) {
            var volumeInfo;
            volumeList.forEach(function(inVolumeInfo) {
              // For extension based providers, provider id is the same as
              // extension id.
              if (inVolumeInfo.providerId === chrome.runtime.id &&
                  inVolumeInfo.fileSystemId === fileSystemId) {
                volumeInfo = inVolumeInfo;
              }
            });
            chrome.test.assertTrue(!!volumeInfo);
            chrome.test.assertFalse(volumeInfo.isReadOnly);
          });
        }));
  },

  // Checks that providing writable=false is processed as read-only.
  function writableFalse() {
    const fileSystemId = 'read-only';
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: fileSystemId,
          displayName: 'read-only.zip',
          writable: false,
        },
        chrome.test.callbackPass(function() {
          chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeList) {
            //  For extension based providers, provider id is the same as
            //  extension id.
            const volumeInfo = volumeList.find(
                inVolumeInfo =>
                    (inVolumeInfo.providerId === chrome.runtime.id &&
                     inVolumeInfo.fileSystemId === fileSystemId));
            chrome.test.assertTrue(!!volumeInfo);
            chrome.test.assertTrue(volumeInfo.isReadOnly);
          });
        }));
  },

  // Checks that providing supportsNotifyTag=false|true persists as requested.
  function supportsNotifyTag() {
    const taggedFilesystemId = 'tagged-fs';
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: taggedFilesystemId,
          displayName: 'tagged-fs-.zip',
          supportsNotifyTag: true,
        },
        chrome.test.callbackPass(function() {
          chrome.fileSystemProvider.get(taggedFilesystemId, fsInfo => {
            chrome.test.assertTrue(fsInfo.supportsNotifyTag);

            chrome.fileSystemProvider.unmount(
                {fileSystemId: taggedFilesystemId},
                chrome.test.callbackPass(() => {}));
          });
        }));

    const nonTaggedFilesystemId = 'non-tagged-fs';
    chrome.fileSystemProvider.mount(
        {
          fileSystemId: nonTaggedFilesystemId,
          displayName: 'tagged-fs-.zip',
          supportsNotifyTag: false,
        },
        chrome.test.callbackPass(function() {
          chrome.fileSystemProvider.get(nonTaggedFilesystemId, fsInfo => {
            chrome.test.assertFalse(fsInfo.supportsNotifyTag);

            chrome.fileSystemProvider.unmount(
                {fileSystemId: nonTaggedFilesystemId},
                chrome.test.callbackPass(() => {}));
          });
        }));
  },

  // Checks is limit for mounted file systems per profile works correctly.
  // Tries to create more than allowed number of file systems. All of the mount
  // requests should succeed, except the last one which should fail with a
  // security error.
  function stressMountTest() {
    var ALREADY_MOUNTED_FILE_SYSTEMS = 6;  // By previous tests.
    var MAX_FILE_SYSTEMS = 16;
    var index = 0;
    var tryNextOne = function() {
      index++;
      if (index < MAX_FILE_SYSTEMS - ALREADY_MOUNTED_FILE_SYSTEMS + 1) {
        var fileSystemId = index + '-stress-test';
        chrome.fileSystemProvider.mount(
            {fileSystemId: fileSystemId, displayName: index + 'th File System'},
            chrome.test.callbackPass(tryNextOne));
      } else {
        chrome.fileSystemProvider.mount(
            {
              fileSystemId: 'over-the-limit-fs-id',
              displayName: 'Over The Limit File System'
            },
            chrome.test.callbackFail('TOO_MANY_OPENED'));
      }
    };
    tryNextOne();
  },

  // Tests if fileManagerPrivate.addProvidedFileSystem() emits the
  // onMountRequested() event.
  function requestMountSuccess() {
    var onMountRequested = chrome.test.callbackPass(
        function(onSuccess, onError) {
          chrome.fileSystemProvider.onMountRequested.removeListener(
              onMountRequested);
        });

    chrome.fileSystemProvider.onMountRequested.addListener(
        onMountRequested);
    chrome.fileManagerPrivate.getProviders(
        chrome.test.callbackPass(function(providers) {
          providers = providers.filter(function(provider) {
            // Filter out native providers.
              return provider.providerId.length > 0 &&
                     provider.providerId[0] != "@";
          });
          chrome.test.assertEq(providers.length, 1);
          chrome.test.assertEq(chrome.runtime.id, providers[0].providerId);
          chrome.test.assertEq(
              chrome.runtime.getManifest().name, providers[0].name);
          chrome.test.assertFalse(providers[0].configurable);
          chrome.test.assertFalse(providers[0].watchable);
          chrome.test.assertFalse(providers[0].multipleMounts);
          chrome.test.assertEq('network', providers[0].source);
        }));

    chrome.fileManagerPrivate.addProvidedFileSystem(
        chrome.runtime.id,
        chrome.test.callbackPass(function() {}));
  }
]);
