// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/**
 * @param {function(...?)} fn
 * @param {...?} args
 * @returns {!Promise<?>}
 */
export async function promisifyWithLastError(fn, ...args) {
  return new Promise((resolve, reject) => {
    fn(...args, (result) => {
      if (chrome.runtime.lastError) {
        reject(chrome.runtime.lastError);
      } else {
        resolve(result);
      }
    });
  });
}

/**
 * Gets volume information for the provided file system.
 *
 * @param {string} fileSystemId Id of the provided file system.
 * @returns {!Promise<?chrome.fileManagerPrivate.VolumeMetadata>} volume
 *     information object in case of success, or null if
 *    not found.
 */
export async function getVolumeInfo(fileSystemId) {
  const volumeList = await new Promise(
      resolve => chrome.fileManagerPrivate.getVolumeMetadataList(resolve));
  for (const volume of volumeList) {
    // For extension backed providers, the provider id is equal to extension
    // id.
    if (volume.providerId === chrome.runtime.id &&
        volume.fileSystemId === fileSystemId) {
      return volume;
    }
  }
  return null;
};

/**
 * Mounts a testing file system and calls the callback in case of a success.
 * On failure, the current test case is failed on an assertion.
 *
 * @returns {!Promise<{fileSystem: !Object, volumeId: string}>} mounted
 *     filesystem and its volume ID.
 */
export async function mountTestFileSystem() {
  const fileSystemId = TestFileSystemProvider.FILESYSTEM_ID;
  await promisifyWithLastError(chrome.fileSystemProvider.mount, {
    fileSystemId,
    displayName: 'Test Filesystem',
    writable: true,
  });
  const volumeInfo = await getVolumeInfo(fileSystemId);
  if (!volumeInfo) {
    throw new Error(`volume not found for filesystem: ${fileSystemId}`);
  }
  const fileSystem = await promisifyWithLastError(
      chrome.fileSystem.requestFileSystem,
      {volumeId: volumeInfo.volumeId, writable: true});
  if (!fileSystem) {
    throw new Error(`filesystem not found for volume: ${volumeInfo.volumeId}`);
  }
  return {fileSystem, volumeId: volumeInfo.volumeId};
};

/**
 * @suppress {checkTypes}
 * @returns {string} request ID
 */
function generateRequestId() {
  return crypto.randomUUID();
}

async function callServiceWorker(commandId, ...args) {
  const requestId = generateRequestId();
  const swContainer = navigator.serviceWorker;
  const sw = (await swContainer.ready).active;

  return new Promise((resolve, reject) => {
    const onReply = (e) => {
      const {requestId, response, error} = e.data;
      if (requestId === requestId) {
        swContainer.removeEventListener('message', onReply);
        if (error) {
          reject(new Error(`service worker returned: ${error}`));
        } else {
          resolve(response);
        }
      }
    };
    setTimeout(() => {
      swContainer.removeEventListener('message', onReply);
      reject(new Error(`request to service worker timed out: ${commandId}`));
    }, 5000);
    swContainer.addEventListener('message', onReply)
    sw.postMessage({requestId, commandId, args});
  });
}

// A proxy to a FileSystemProvider instance running in a service worker to be
// called from test code.
export const remoteProvider = {
  /**
   * @param {!Object<string, !Object>} files
   * @returns {!Promise<void>}
   */
  addFiles: async (files) => callServiceWorker('addFiles', files),
  /**
   * @param {string} filePath
   * @returns {!Promise<string>}
   */
  getFileContents: async (filePath) =>
      callServiceWorker('getFileContents', filePath),
  /**
   * @returns {!Promise<void>}
   */
  resetCallQueues: async () => callServiceWorker('resetCallQueues'),
  /**
   * @param {string} funcName
   * @returns {!Promise<!Object>}
   */
  waitForCall: async (funcName) => callServiceWorker('waitForCall', funcName),
};
