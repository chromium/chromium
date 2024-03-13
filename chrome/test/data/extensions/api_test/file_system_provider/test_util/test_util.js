// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Namespace for testing utilities.
var test_util = {};

/**
 * @type {string}
 * @const
 */
test_util.FILE_SYSTEM_ID = 'vanilla.txt';

/**
 * @type {string}
 * @const
 */
test_util.FILE_SYSTEM_NAME = 'Vanilla';

/**
 * @type {FileSystem}
 */
test_util.fileSystem = null;

/**
 * @type {?string}
 */
test_util.volumeId = null;

/**
 * Default metadata. Used by onMetadataRequestedDefault(). The key is a full
 * path, and the value, a MetadataEntry object.
 *
 * @type {Object<string, Object>}
 */
test_util.defaultMetadata = {
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
test_util.openedFiles = {};

/**
 * Gets volume information for the provided file system.
 *
 * @param {string} fileSystemId Id of the provided file system.
 * @param {function(Object)} callback Callback to be called on result, with the
 *     volume information object in case of success, or null if not found.
 */
test_util.getVolumeInfo = function(fileSystemId, callback) {
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
test_util.mountFileSystem = function(callback, opt_options) {
  var options = {
    fileSystemId: test_util.FILE_SYSTEM_ID,
    displayName: test_util.FILE_SYSTEM_NAME,
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

        test_util.getVolumeInfo(options.fileSystemId, function(volumeInfo) {
          chrome.test.assertTrue(!!volumeInfo);
          chrome.fileSystem.requestFileSystem(
              {
                volumeId: volumeInfo.volumeId,
                writable: true
              },
              function(inFileSystem) {
                chrome.test.assertTrue(!!inFileSystem);
                test_util.fileSystem = inFileSystem;
                test_util.volumeId = volumeInfo.volumeId;
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
test_util.onGetMetadataRequestedDefault = function(
    options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (!(options.entryPath in test_util.defaultMetadata)) {
    onError('NOT_FOUND');
    return;
  }

  onSuccess(test_util.defaultMetadata[options.entryPath]);
};

/**
 * Default implementation for the file open request event. Further file
 * operations will be associated with the <code>requestId</code>.
 *
 * @param {OpenFileRequestedOptions} options Options.
 * @param {function()} onSuccess Success callback.
 * @param {function(string)} onError Error callback.
 */
test_util.onOpenFileRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  var metadata = test_util.defaultMetadata[options.filePath];
  if (metadata && !metadata.is_directory) {
    test_util.openedFiles[options.requestId] = options.filePath;
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
test_util.onCloseFileRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID ||
      !test_util.openedFiles[options.openRequestId]) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  delete test_util.openedFiles[options.openRequestId];
  onSuccess();
};

/**
 * Default implementation for the file create request event.
 *
 * @param {CreateFileRequestedOptions} options Options.
 * @param {function(Object)} onSuccess Success callback
 * @param {function(string)} onError Error callback with an error code.
 */
test_util.onCreateFileRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.filePath === '/') {
    onError('INVALID_OPERATION');
    return;
  }

  if (options.filePath in test_util.defaultMetadata) {
    onError('EXISTS');
    return;
  }

  test_util.defaultMetadata[options.filePath] = {
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
test_util.onAddWatcherRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath in test_util.defaultMetadata) {
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
test_util.onRemoveWatcherRequested = function(options, onSuccess, onError) {
  if (options.fileSystemId !== test_util.FILE_SYSTEM_ID) {
    onError('SECURITY');  // enum ProviderError.
    return;
  }

  if (options.entryPath in test_util.defaultMetadata) {
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
test_util.toExternalEntry = function(entry) {
  return new Promise(
      function(fulfill, reject) {
        chrome.fileManagerPrivate.resolveIsolatedEntries(
            [entry],
            function(entries) {
              fulfill(entries && entries.length ? entries[0] : null);
            });
      });
};
