// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A blocking queue to allow the remote test interact with the FSP
 * implementation asynchronously.
 */
class Queue {
  constructor() {
    /** @type {!Array<!Object>} */
    this.items = [];
    /** @type {!Array<function(!Object)>} */
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
    this.items.push(item);
  }

  /**
   * Pops the first item from the queue. If the queue is empty, will wait until
   * an item is available.
   *
   * @returns {!Object}
   */
  async pop() {
    if (this.items.length > 0) {
      return this.items.shift();
    }
    return new Promise(resolve => {
      this.readers.push(resolve);
    });
  }
};

export class TestFileSystemProvider {
  constructor(fileSystemId) {
    this.fileSystemId = fileSystemId;
    /**
     * Filesystem contents (data and metadata). The key is a full path, and the
     * value is an object containing metadata and file contents.
     *
     * @private {!Object<string, !Object>}
     */
    this.files = {
      '/': {
        metadata: {
          isDirectory: true,
          name: '',
          size: 0,
          modificationTime: new Date(2014, 4, 28, 10, 39, 15)
        },
      },
      // Read error
      ['/' + TestFileSystemProvider.FILE_FAIL]: {
        metadata: {
          isDirectory: false,
          name: TestFileSystemProvider.FILE_FAIL,
          size: TestFileSystemProvider.INITIAL_TEXT.length,
          modificationTime: new Date(2014, 1, 25, 7, 36, 12),
        },
        contents: TestFileSystemProvider.INITIAL_TEXT,
      },
      // Read blocks indefinitely
      ['/' + TestFileSystemProvider.FILE_BLOCKS_FOREVER]: {
        metadata: {
          isDirectory: false,
          name: TestFileSystemProvider.FILE_BLOCKS_FOREVER,
          size: TestFileSystemProvider.INITIAL_TEXT.length,
          modificationTime: new Date(2014, 1, 26, 8, 37, 13),
        },
        contents: TestFileSystemProvider.INITIAL_TEXT,
      },
    };

    /**
     * Map of opened files, from a `openRequestId` to `filePath`.
     *
     * @private {!Object<number, string>}
     */
    this.openedFiles = {};

    /**
     * A queue of recorded calls per FSP function.
     *
     * @private {!Object<string, !Queue>}
     */
    this.callQueues = {};
  }

  setUpProviderListeners() {
    chrome.fileSystemProvider.onGetMetadataRequested.addListener(
        this.onGetMetadataRequested.bind(this));
    chrome.fileSystemProvider.onOpenFileRequested.addListener(
        this.onOpenFileRequested.bind(this));
    chrome.fileSystemProvider.onCloseFileRequested.addListener(
        this.onCloseFileRequested.bind(this));
    chrome.fileSystemProvider.onCreateFileRequested.addListener(
        this.onCreateFileRequested.bind(this));
    chrome.fileSystemProvider.onWriteFileRequested.addListener(
        this.onWriteFileRequested.bind(this));
    chrome.fileSystemProvider.onAbortRequested.addListener(
        this.onAbortRequested.bind(this));
  }

  setUpCommandListener(serviceWorker) {
    serviceWorker.onmessage = (e) => {
      const {requestId, commandId, args} = e.data;
      e.waitUntil((
          /** @suppress {checkTypes} */
          async () => {
            const result = {requestId};
            try {
              if (commandId in this) {
                result.response = await this[commandId](...args);
              } else {
                result.error = `unhandled: ${commandId}`;
              }
            } catch (error) {
              result.error = error.toString();
            }
            e.source.postMessage(result);
          })());
    };
  }

  /**
   * Called by the test. Add files to the provider's filesystem.
   * @param {!Object<string, !Object>} files
   */
  addFiles(files) {
    // Restore Date objects after receiving data via postMessage.
    for (const file of Object.values(files)) {
      file.metadata.modificationTime = new Date(file.metadata.modificationTime);
    }
    this.files = {...this.files, ...files};
  }

  /**
   * Called by the test. Gets contents of a given file.
   *
   * @param {string} filePath
   * @returns The current text contents of the file.
   */
  getFileContents(filePath) {
    return this.files[filePath].contents;
  }

  /**
   * Called by the test. Gets the least recent call recorded for an FSP
   * function. Will block until there is at least one in the queue.
   *
   * @param {string} funcName FSP function name.
   * @returns {!Object} the 'options' argument passed to the FSP call.
   */
  async waitForCall(funcName) {
    return this.getCallQueue(funcName).pop();
  }

  /**
   * Called by the FSP. Adds a record of a function call to the queue. The test
   * will read from this queue to wait for a specific FSP call to happen.
   *
   * @param {string} funcName FSP function name.
   * @param {!Object} optionsArg the 'options' arguments passed to this FSP
   *     call.
   */
  recordCall(funcName, optionsArg) {
    this.getCallQueue(funcName).push(optionsArg);
  }

  /**
   * Gets or creates a call queue for a function.
   *
   * @param {string} funcName
   * @returns {!Queue}
   */
  getCallQueue(funcName) {
    if (!(funcName in this.callQueues)) {
      this.callQueues[funcName] = new Queue();
    }
    return this.callQueues[funcName];
  }

  /**
   * Clears all the call queues for all functions.
   */
  resetCallQueues() {
    this.callQueues = {};
  }

  /**
   * FSP: implementation for the metadata request event.
   *
   * @param {chrome.fileSystemProvider.GetMetadataRequestedOptions} options
   *     Options.
   * @param {function(!Object)} onSuccess Success callback with metadata passed
   *     an argument.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback with an error code.
   */
  onGetMetadataRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (!(options.entryPath in this.files)) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    onSuccess(this.files[options.entryPath].metadata);
  };

  /**
   * FSP: implementation for the file open request event. Further file
   * operations will be associated with the <code>requestId</code>.
   *
   * @param {!chrome.fileSystemProvider.OpenFileRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback.
   */
  onOpenFileRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    const metadata = this.files[options.filePath].metadata;
    if (metadata && !metadata.is_directory) {
      this.openedFiles[options.requestId] = options.filePath;
      onSuccess();
    } else {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
    }
  };

  /**
   * FSP: implementation for the file close request event. The file,
   * previously opened with <code>openRequestId</code> will be closed.
   *
   * @param {!chrome.fileSystemProvider.CloseFileRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback.
   */
  onCloseFileRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId ||
        !this.openedFiles[options.openRequestId]) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    delete this.openedFiles[options.openRequestId];
    onSuccess();
  };

  /**
   * FSP: implementation for the file create request event.
   *
   * @param {!chrome.fileSystemProvider.CreateFileRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback with an error code.
   */
  onCreateFileRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.filePath === '/') {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    if (options.filePath in this.files) {
      onError(chrome.fileSystemProvider.ProviderError.EXISTS);
      return;
    }

    this.files[options.filePath] = {
      metadata: {
        isDirectory: false,
        name: options.filePath.split('/').pop(),
        size: 0,
        modificationTime: new Date()
      },
      contents: '',
    };

    onSuccess();
  };

  /**
   * FSP: requests writing contents to a file, previously opened with <code>
   * openRequestId</code>.
   *
   * @param {!chrome.fileSystemProvider.WriteFileRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback.
   */
  onWriteFileRequested(options, onSuccess, onError) {
    this.recordCall('onWriteFileRequested', options);

    if (options.fileSystemId !== this.fileSystemId ||
        !this.openedFiles[options.openRequestId]) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    const filePath = this.openedFiles[options.openRequestId];
    if (!(filePath in this.files)) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const file = this.files[filePath];
    const metadata = file.metadata;

    if (filePath === '/' + TestFileSystemProvider.FILE_FAIL) {
      onError(chrome.fileSystemProvider.ProviderError.FAILED);
      return;
    }

    if (filePath === '/' + TestFileSystemProvider.FILE_BLOCKS_FOREVER) {
      // Do not call any callback to simulate a very slow network connection.
      return;
    }

    // Writing beyond the end of the file.
    if (options.offset > metadata.size) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    // Create an array with enough space for new data.
    const oldArray = new TextEncoder().encode(file.contents || '');
    const newLength =
        Math.max(oldArray.length, options.offset + options.data.byteLength);
    const newArray = new Uint8Array(new ArrayBuffer(newLength));
    // Write existing data and new data.
    newArray.set(oldArray, 0);
    newArray.set(new Uint8Array(options.data), options.offset);
    // Save the new file as text.
    const newContents = new TextDecoder().decode(newArray);
    file.contents = newContents;
    metadata.size = newContents.length;
    onSuccess();
  }

  onAbortRequested(options, onSuccess, onError) {
    this.recordCall('onAbortRequested', options);
    if (options.fileSystemId !== this.fileSystemId) {
      onError('SECURITY');  // enum ProviderError.
      return;
    }

    onSuccess();
  }

  /**
   * FSP: implementation for adding an entry watcher.
   *
   * @param {!chrome.fileSystemProvider.AddWatcherRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback.
   * @param {function(string)} onError Error callback with an error code.
   */
  onAddWatcherRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError('SECURITY');  // enum ProviderError.
      return;
    }

    if (options.entryPath in this.files) {
      onSuccess();
      return;
    }

    onError('NOT_FOUND');  // enum ProviderError.
  };

  /**
   * FSP: implementation for removing an entry watcher.
   *
   * @param {!chrome.fileSystemProvider.AddWatcherRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback.
   * @param {function(string)} onError Error callback with an error code.
   */
  onRemoveWatcherRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError('SECURITY');  // enum ProviderError.
      return;
    }

    if (options.entryPath in this.files) {
      onSuccess();
      return;
    }

    onError('NOT_FOUND');  // enum ProviderError.
  };
};

/**
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILESYSTEM_ID = 'test-fs';

/**
 * Reads and writes on this file always fail.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_FAIL = 'fail.txt';

/**
 * Reads and writes on this file never finish.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_BLOCKS_FOREVER = 'blocks-forever.txt';

/**
 * Initial contents of default testing files.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.INITIAL_TEXT = 'Hello world. How are you today?';
