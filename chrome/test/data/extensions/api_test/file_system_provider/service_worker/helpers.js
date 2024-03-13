// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {TestFileSystemProvider} from '/_test_resources/api_test/file_system_provider/service_worker/provider.js';

/** A blocking queue implementation.  */
export class Queue {
  constructor() {
    /**
     * Items currently in the queue.
     * @private {!Array<!Object>}
     */
    this.items_ = [];
    /**
     * Readers waiting for an item to be pushed to the queue.
     * @private {!Array<function(!Object)>}
     */
    this.readers = [];
  }

  /**
   * Pushes an item into the queue and unblocks the first waiting reader if
   * there are any. This method returns immediately and will never block.
   *
   * @param {!Object} item
   */
  push(item) {
    if (this.readers.length > 0) {
      this.readers.shift()(item);
      return;
    }
    this.items_.push(item);
  }

  /**
   * Pops the first item from the queue. If the queue is empty, will wait until
   * an item is available.
   *
   * @returns {!Object}
   */
  async pop() {
    if (this.items_.length > 0) {
      return this.items_.shift();
    }
    return new Promise(resolve => {
      this.readers.push(resolve);
    });
  }

  clear() {
    this.items_ = [];
  }

  /** @returns {number} */
  size() {
    return this.items_.length;
  }
};


/**
 * @param {function(...?)} fn
 * @param {...?} args
 * @returns {!Promise<?>}
 */
export async function promisifyWithLastError(fn, ...args) {
  return new Promise((resolve, reject) => {
    fn(...args, (result) => {
      const error = chrome.runtime.lastError;
      if (error) {
        reject(new Error(error.message));
      } else {
        resolve(result);
      }
    });
  });
}

/**
 * Catch error thrown in an async function.
 *
 * @param {!Promise<?>} promise
 * @returns {?Object} thrown error, or null if the function returns
 *    successfully. Function's return value is discarded.
 */
export async function catchError(promise) {
  try {
    await promise;
    return null;
  } catch (e) {
    return e;
  }
}

/**
 * @param {!FileEntry} fileEntry
 * @returns {!Promise<!FileWriter>}
 */
 export async function createWriter(fileEntry) {
  return new Promise(
      (resolve, reject) => fileEntry.createWriter(resolve, reject));
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
  const volumeList = await promisifyWithLastError(
      chrome.fileManagerPrivate.getVolumeMetadataList);
  for (const volume of volumeList) {
    if (volume.providerId === chrome.runtime.id &&
        volume.fileSystemId === fileSystemId &&
        volume.diskFileSystemType !== 'fusebox') {
      return volume;
    }
  }
  throw new Error(`volume not found: ${fileSystemId}`);
};

/**
 * Wrappers for chrome.fileSystemProvider.* are still needed for Closure to
 * work, as it's not aware they are returning promises if callbacks are omitted.
 * @returns {!Promise<!Array<!chrome.fileSystemProvider.FileSystemInfo>>}
 * @suppress {checkTypes}
 */
export async function getAllFsInfos() {
  return chrome.fileSystemProvider.getAll();
}

/**
 * @param {!FileEntry|!DirectoryEntry} entry
 * @returns {!Promise<!Metadata>}
 */
 export async function getMetadata(entry) {
  return new Promise((resolve, reject) => entry.getMetadata(resolve, reject));
}

/**
 * Async wrapper.
 * @param {string} fileSystemId
 * @returns {!Promise<void>}
 * @suppress {checkTypes}
 */
export async function unmount(fileSystemId) {
  return chrome.fileSystemProvider.unmount({fileSystemId});
}

/**
 * Async wrapper.
 * @param {string} fileSystemId
 * @returns {!Promise<!chrome.fileSystemProvider.FileSystemInfo>}
 * @suppress {checkTypes}
 */
export async function getFsInfoById(fileSystemId) {
  return chrome.fileSystemProvider.get(fileSystemId);
}

/**
 * @param {{
 *    openedFilesLimit: (number|undefined),
 *    supportsNotifyTag:(boolean|undefined)
 * }=} optionsOverride
 * @returns {!Promise<{fileSystem: !FileSystem, volumeInfo:
 *     !chrome.fileManagerPrivate.VolumeMetadata}>} information about the
 *     mounted filesystem instance.
 */
async function mount(optionsOverride) {
  const options = {
    fileSystemId: TestFileSystemProvider.FILESYSTEM_ID,
    displayName: 'Test Filesystem',
    writable: true,
    ...optionsOverride,
  };
  await promisifyWithLastError(chrome.fileSystemProvider.mount, options);
  const volumeInfo = await getVolumeInfo(options.fileSystemId);
  if (!volumeInfo) {
    throw new Error(`volume not found for filesystem: ${options.fileSystemId}`);
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
    const {fileSystem, volumeInfo} = await mount({openedFilesLimit});
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
 * @param {{
 *    openedFilesLimit: (number|undefined),
 *    supportsNotifyTag: (boolean|undefined)
 * }=} optionsOverride
 * @return {!Promise<!MountedTestFileSystem>}
 */
export async function mountTestFileSystem(optionsOverride) {
  const {fileSystem, volumeInfo} = await mount(optionsOverride);
  return new MountedTestFileSystem(fileSystem, volumeInfo);
}

/**
 * A proxy to a FileSystemProvider instance running in a different context (or
 * different extension) to be called from test code. All the calls and arguments
 * are forwarded as is to the test provider, see corresponding functions in
 * FileSystemProvider for descriptions.
 */
export class ProviderProxy {
  constructor(extensionId) {
    /**
     * Target extension ID to send messages to.
     *
     * @private {string}
     */
    this.extensionId_ = extensionId;
  }

  /**
   * @param {!Object<string, !Object>} files
   * @returns {!Promise<void>}
   */
  async addFiles(files) {
    return this.callProvider('addFiles', files);
  }

  /**
   * @param {number} requestId
   * @returns {!Promise<void>}
   */
  async continueRequest(requestId) {
    return this.callProvider('continueRequest', requestId);
  }

  /**
   * @param {string} eventName
   * @returns {!Promise<number>}
   */
  async getEventCount(eventName) {
    return this.callProvider('getEventCount', eventName);
  }

  /**
   * @param {string} filePath
   * @returns {!Promise<string>}
   */
  async getFileContents(filePath) {
    return this.callProvider('getFileContents', filePath);
  }

  /**
   * @returns {!Promise<number>}
   */
  async getOpenedFiles() {
    return this.callProvider('getOpenedFiles');
  }

  /**
   * @param {string} entryPath
   * @param {boolean} recursive
   * @param {string} tag
   */
  async triggerNotify(entryPath, recursive, tag) {
    return this.callProvider('triggerNotify', entryPath, recursive, tag);
  }

  /**
   * @param {string} url
   * @returns {!Promise<number>}
   */
  async openTab(url) {
    return this.callProvider('openTab', url);
  }

  /** @param {number} tabId */
  async closeTab(tabId) {
    return this.callProvider('closeTab', tabId);
  }

  /**
   * @param {string} url
   * @returns {!Promise<number>}
   */
  async openWindow(url) {
    return this.callProvider('openWindow', url);
  }

  /**
   * @param {string} key
   * @param {?} value
   */
  async setConfig(key, value) {
    return this.callProvider('setConfig', key, value);
  }

  /**
   * @param {string} handlerName
   * @param {boolean} enabled
   */
  async setHandlerEnabled(handlerName, enabled) {
    return this.callProvider('setHandlerEnabled', handlerName, enabled);
  }

  /**
   * @returns {!Promise<void>}
   */
  async resetState() {
    return this.callProvider('resetState');
  }

  /**
   * @param {string} funcName
   * @returns {!Promise<!Object>}
   */
  async waitForEvent(funcName) {
    return this.callProvider('waitForEvent', funcName);
  }

  async callProvider(commandId, ...args) {
    const {response, error} = await promisifyWithLastError(
        chrome.runtime.sendMessage, this.extensionId_, {commandId, args},
        /*options=*/ {});
    if (error) {
      throw new Error(`service worker returned: ${error}`);
    }
    return response;
  }
};

/**
 * Default provider proxy: sends messages to the same extension (but could still
 * be in a different context, i.e. foreground page vs service worker).
 */
export const remoteProvider = new ProviderProxy(chrome.runtime.id);
