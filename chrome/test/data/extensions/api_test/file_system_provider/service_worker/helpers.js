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
 * @param {number=} openedFilesLimit Limit of opened files at once. If 0 or
 *     unspecified, then not limited.
 * @returns {!Promise<{fileSystem: !FileSystem, volumeInfo:
 *     !chrome.fileManagerPrivate.VolumeMetadata}>} information about the
 *     mounted filesystem instance.
 */
async function mount(openedFilesLimit) {
  const fileSystemId = TestFileSystemProvider.FILESYSTEM_ID;
  const options = {
    fileSystemId,
    displayName: 'Test Filesystem',
    writable: true,
  };
  if (openedFilesLimit) {
    options.openedFilesLimit = openedFilesLimit;
  }
  await promisifyWithLastError(chrome.fileSystemProvider.mount, options);
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
  return {fileSystem, volumeInfo};
};

/**
 * @param {!FileEntry} fileEntry
 * @returns {!Promise<!File>}
 */
export async function openFile(fileEntry) {
  return new Promise((resolve, reject) => fileEntry.file(resolve, reject));
}

/**
 * @param {!Blob} blob
 * @returns {!Promise<string>}
 */
export async function readTextFromBlob(blob) {
  const {promise} = startReadTextFromBlob(blob);
  return promise;
}

/**
 * @param {!Blob} blob
 * @returns {{promise: !Promise<string>, reader: !FileReader}}
 */
export function startReadTextFromBlob(blob) {
  const reader = new FileReader();
  const promise = new Promise((resolve, reject) => {
    reader.onload = e => resolve(reader.result);
    reader.onerror = e => reject(reader.error);
    reader.onabort = e => reject(reader.error);
    reader.readAsText(blob);
  });
  return {reader, promise};
}

export class MountedTestFileSystem {
  /**
   * @param {!FileSystem} fileSystem
   * @param {!chrome.fileManagerPrivate.VolumeMetadata} volumeInfo
   */
  constructor(fileSystem, volumeInfo) {
    this.fileSystem = fileSystem;
    this.volumeInfo = volumeInfo;
  }

  /**
   * Unmount and mount the test filesystem with a new open file limit.
   *
   * @param {number} openedFilesLimit
   */
  async remount(openedFilesLimit) {
    await promisifyWithLastError(chrome.fileSystemProvider.unmount, {
      fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
    });
    const {fileSystem, volumeInfo} = await mount(openedFilesLimit);
    this.fileSystem = fileSystem;
    this.volumeInfo = volumeInfo;
  }

  /**
   * Get a file entry from the root of the mounted filesystem.
   *
   * @param {string} path
   * @param {{create: (boolean|undefined), exclusive: (boolean|undefined)}}
   *     options
   * @returns {!Promise<!FileEntry>}
   */
  async getFileEntry(path, options) {
    return new Promise(
        (resolve, reject) =>
            this.fileSystem.root.getFile(path, options, resolve, reject));
  }

  /**
   * Get a directory entry from the root of the mounted filesystem.
   *
   * @param {string} path
   * @param {{create: (boolean|undefined), exclusive: (boolean|undefined)}}
   *  options
   * @returns {!Promise<!DirectoryEntry>}
   */
  async getDirectoryEntry(path, options) {
    return new Promise(
        (resolve, reject) =>
            this.fileSystem.root.getDirectory(path, options, resolve, reject));
  }
};

/**
 * Create a mounted test filesystem instance.
 *
 * @param {number=} openedFilesLimit Limit of opened files at once. If 0 or
 *     unspecified, then not limited.
 * @return {!Promise<!MountedTestFileSystem>}
 */
export async function mountTestFileSystem(openedFilesLimit) {
  const {fileSystem, volumeInfo} = await mount(openedFilesLimit);
  return new MountedTestFileSystem(fileSystem, volumeInfo);
}

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
      reject(new Error(
          `request to service worker timed out: ${commandId} args: ` +
          JSON.stringify(args)));
    }, 5000);
    swContainer.addEventListener('message', onReply)
    sw.postMessage({requestId, commandId, args});
  });
}

/**
 * A proxy to a FileSystemProvider instance running in a service worker to be
 * called from test code. All the calls and arguments are forwarded as is to the
 * test provider, see corresponding functions in FileSystemProvider for
 * descriptions.
 */
export const remoteProvider = {
  /**
   * @param {!Object<string, !Object>} files
   * @returns {!Promise<void>}
   */
  addFiles: async (files) => callServiceWorker('addFiles', files),
  /**
   * @param {number} requestId
   * @returns {!Promise<void>}
   */
  continueRequest: async (requestId) =>
      callServiceWorker('continueRequest', requestId),
  /**
   * @param {string} eventName
   * @returns {!Promise<number>}
   */
  getEventCount: async (eventName) =>
      callServiceWorker('getEventCount', eventName),
  /**
   * @param {string} filePath
   * @returns {!Promise<string>}
   */
  getFileContents: async (filePath) =>
      callServiceWorker('getFileContents', filePath),
  /**
   * @returns {!Promise<number>}
   */
  getOpenedFiles: async () => callServiceWorker('getOpenedFiles'),
  /**
   * @param {string} key
   * @param {?} value
   */
  setConfig: async (key, value) => callServiceWorker('setConfig', key, value),
  /**
   * @param {string} handlerName
   * @param {boolean} enabled
   */
  setHandlerEnabled: async (handlerName, enabled) =>
      callServiceWorker('setHandlerEnabled', handlerName, enabled),
  /**
   * @returns {!Promise<void>}
   */
  resetState: async () => callServiceWorker('resetState'),
  /**
   * @param {string} funcName
   * @returns {!Promise<!Object>}
   */
  waitForEvent: async (funcName) => callServiceWorker('waitForEvent', funcName),
};
