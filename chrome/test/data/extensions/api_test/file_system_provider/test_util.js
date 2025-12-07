// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {string}
 * @const
 */
export const FILE_SYSTEM_ID = 'vanilla.txt';

/**
 * @type {string}
 * @const
 */
export const FILE_SYSTEM_NAME = 'Vanilla';

/**
 * @type {FileSystem}
 */
export let fileSystem = null;

/**
 * @type {?string}
 */
export let volumeId = null;

/**
 * Default metadata. Used by onMetadataRequestedDefault(). The key is a full
 * path, and the value, a MetadataEntry object.
 *
 * @type {Object<string, Object>}
 */
export const defaultMetadata = {
  '/': {
    isDirectory: true,
    name: '',
    size: 0,
    modificationTime: new Date(2014, 4, 28, 10, 39, 15)
  }
};

/**
 * Map of opened files, from a <code>openRequestId</code> to <code>filePath
 * </code>.
 *
 * @type {Object<number, string>}
 */
export const openedFiles = {};

/**
 * Gets volume information for the provided file system.
 *
 * @param {string} fileSystemId Id of the provided file system.
 * @param {function(Object)} callback Callback to be called on result, with the
 *     volume information object in case of success, or null if not found.
 */
export const getVolumeInfo = function(fileSystemId, callback) {
  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeList) {
    for (var i = 0; i < volumeList.length; i++) {
      // For extension backed providers, the provider id is equal to extension
      // id.
      if (volumeList[i].providerId === chrome.runtime.id &&
          volumeList[i].fileSystemId === fileSystemId &&
          volumeList[i].diskFileSystemType !== 'fusebox') {
        callback(volumeList[i]);
        return;
      }
    }
    callback(null);
  });
};

/**
 * Mounts a testing file system and calls the callback in case of a success.
 * On failure, the current test case is failed on an assertion.
 *
 * @param {function()} callback Success callback.
 * @param {Object=} opt_options Optional extra options.
 */
export const mountFileSystem = function(callback, opt_options) {
  var options = {
    fileSystemId: FILE_SYSTEM_ID,
    displayName: FILE_SYSTEM_NAME,
    writable: true
  };

  // If any extra options are provided then merge then. They may override the
  // default ones.
  if (opt_options) {
    for (var key in opt_options)
      options[key] = opt_options[key];
  }

  chrome.fileSystemProvider.mount(
      options,
      function() {
        // Note that chrome.test.callbackPass() cannot be used, as it would
        // prematurely finish the test at the setUp() stage.
        if (chrome.runtime.lastError)
          chrome.test.fail(chrome.runtime.lastError.message);

        getVolumeInfo(options.fileSystemId, function(volumeInfo) {
          chrome.test.assertTrue(!!volumeInfo);
          chrome.fileSystem.requestFileSystem(
              {
                volumeId: volumeInfo.volumeId,
                writable: true
              },
              function(inFileSystem) {
                chrome.test.assertTrue(!!inFileSystem);
                fileSystem = inFileSystem;
                volumeId = volumeInfo.volumeId;
                callback();
              });
        });
      });
};

/**
 * Default implementation for the metadata request event.
 *
 * @param {GetMetadataRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback with metadata passed
 *     an argument.
 * @param {function(string)} onError Error callback with an error code.
 */
export const onGetMetadataRequestedDefault = function(
    options, onSuccess, onError) {
  if (options.fileSystemId !== FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (!(options.entryPath in defaultMetadata)) {
    onError('NOT_FOUND');
    return;
  }

  onSuccess(defaultMetadata[options.entryPath]);
};

/**
 * Default implementation for the file open request event. Further file
 * operations will be associated with the <code>requestId</code>.
 *
 * @param {OpenFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
export const onOpenFileRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  var metadata = defaultMetadata[options.filePath];
  if (metadata && !metadata.is_directory) {
    openedFiles[options.requestId] = options.filePath;
    onSuccess();
  } else {
    onError('NOT_FOUND');  // enum ProviderError.
  }
};

/**
 * Default implementation for the file close request event. The file, previously
 * opened with <code>openRequestId</code> will be closed.
 *
 * @param {CloseFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
export const onCloseFileRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== FILE_SYSTEM_ID ||
      !openedFiles[options.openRequestId]) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  delete openedFiles[options.openRequestId];
  onSuccess();
};

/**
 * Default implementation for the file create request event.
 *
 * @param {CreateFileRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback
 * @param {function(string)} onError Error callback with an error code.
 */
export const onCreateFileRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.filePath === '/') {
    onError('INVALID_OPERATION');
    return;
  }

  if (options.filePath in defaultMetadata) {
    onError('EXISTS');
    return;
  }

  defaultMetadata[options.filePath] = {
    isDirectory: false,
    name: options.filePath.split('/').pop(),
    size: 0,
    modificationTime: new Date()
  };

  onSuccess();  // enum ProviderError.
};

/**
 * Default implementation for adding an entry watcher.
 *
 * @param {AddWatcherRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback with an error code.
 */
export const onAddWatcherRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath in defaultMetadata) {
    onSuccess();
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
};

/**
 * Default implementation for removing an entry watcher.
 *
 * @param {AddWatcherRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback with an error code.
 */
export const onRemoveWatcherRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath in defaultMetadata) {
    onSuccess();
    return;
  }

  onError('NOT_FOUND');  // enum ProviderError.
};

/**
 * Temporary method for converting an isolated entry to an external one.
 * TODO(mtomasz): Remove after transition to isolated file systems is completed.
 *
 * @param {Entry} entry Isolated entry.
 * @return {!Promise.<Entry>} Promise with an external entry, or null in case of
 *     on error.
 */
export const toExternalEntry = function(entry) {
  return new Promise(
      function(fulfill, reject) {
        chrome.fileManagerPrivate.resolveIsolatedEntries(
            [entry],
            function(entries) {
              fulfill(entries && entries.length ? entries[0] : null);
            });
      });
};
