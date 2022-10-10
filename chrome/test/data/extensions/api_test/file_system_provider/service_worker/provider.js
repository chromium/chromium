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
      // Read and write blocks indefinitely.
      ['/' + TestFileSystemProvider.FILE_BLOCKS_FOREVER]: {
        metadata: {
          isDirectory: false,
          name: TestFileSystemProvider.FILE_BLOCKS_FOREVER,
          size: TestFileSystemProvider.INITIAL_TEXT.length,
          modificationTime: new Date(2014, 1, 26, 8, 37, 13),
        },
        contents: TestFileSystemProvider.INITIAL_TEXT,
      },
      // Open blocks until unblocked manually.
      ['/' + TestFileSystemProvider.FILE_STALL_OPEN]: {
        metadata: {
          isDirectory: false,
          name: TestFileSystemProvider.FILE_STALL_OPEN,
          size: TestFileSystemProvider.INITIAL_TEXT.length,
          modificationTime: new Date(2014, 1, 26, 8, 37, 13),
        },
        contents: TestFileSystemProvider.INITIAL_TEXT,
      },
      // Read blocks until unblocked manually.
      ['/' + TestFileSystemProvider.FILE_STALL_READ]: {
        metadata: {
          isDirectory: false,
          name: TestFileSystemProvider.FILE_STALL_READ,
          size: TestFileSystemProvider.INITIAL_TEXT.length,
          modificationTime: new Date(2014, 1, 26, 8, 37, 13),
        },
        contents: TestFileSystemProvider.INITIAL_TEXT,
      },
      // Read returns data in chunks.
      ['/' + TestFileSystemProvider.FILE_READ_SUCCESS]: {
        metadata: {
          isDirectory: false,
          name: TestFileSystemProvider.FILE_READ_SUCCESS,
          size: TestFileSystemProvider.INITIAL_TEXT.length,
          modificationTime: new Date(2014, 1, 25, 7, 36, 12)
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
     * Records max number of opened files any time a file is opened.
     *
     * @private {number}
     */
    this.maxOpenedFiles = 0;

    /**
     * A queue of recorded event per event name.
     *
     * @private {!Object<string, !Queue>}
     */
    this.eventQueues = {};

    /**
     * A map of FSP handler name to a Bound FSP handler, so handlers can be
     * added or removed mid-test.
     *
     * @private {!Object<string, !Function>}
     */
    this.handlers = {};

    /**
     * A map of FSP requests that's been deliberately stalled. The key is FSP
     * request ID, and the value are the arguments passed to the request
     * handler.
     *
     * @private {!Object<number, function()>}
     */
    this.stalledRequests = {};
  }

  setUpProviderListeners() {
    this.setHandlerEnabled('onAbortRequested', true);
    this.setHandlerEnabled('onAddWatcherRequested', true);
    this.setHandlerEnabled('onCloseFileRequested', true);
    this.setHandlerEnabled('onCopyEntryRequested', true);
    this.setHandlerEnabled('onCreateFileRequested', true);
    this.setHandlerEnabled('onGetMetadataRequested', true);
    this.setHandlerEnabled('onOpenFileRequested', true);
    this.setHandlerEnabled('onReadFileRequested', true);
    this.setHandlerEnabled('onRemoveWatcherRequested', true);
    this.setHandlerEnabled('onWriteFileRequested', true);
  }

  /**
   * Enable or disable the listener for a named FSP event (e.g.
   * onOpenFileRequested, onAbortRequested).
   *
   * @suppress {checkTypes}
   */
  setHandlerEnabled(handlerName, enabled) {
    if (!(handlerName in this)) {
      throw new Error(
        `${this.constructor.name} does not implement ${handlerName}`);
    }
    if (!(handlerName in this.handlers)) {
      this.handlers[handlerName] = this[handlerName].bind(this);
    }
    if (enabled) {
      chrome.fileSystemProvider[handlerName].addListener(
          this.handlers[handlerName]);
    } else {
      chrome.fileSystemProvider[handlerName].removeListener(
          this.handlers[handlerName]);
    }
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
   * Called by the test. Returns the number of files that are currently open.
   *
   * @returns {number}
   */
  getOpenedFiles() {
    return Object.keys(this.openedFiles).length;
  }

  /**
   * Called by the test. Gets the least recent event recorded for a given event
   * name. Will block until there is at least one in the queue.
   *
   * @param {string} funcName FSP function name.
   * @returns {!Object} the 'options' argument passed to the FSP call.
   */
  async waitForEvent(funcName) {
    return this.getEventQueue(funcName).pop();
  }

  /**
   * Called by the FSP. Adds a record of a function call to the queue. The test
   * will read from this queue to wait for a specific FSP call to happen.
   *
   * @param {string} name event name.
   * @param {!Object} arg additional data associated with the event.
   */
  recordEvent(name, arg) {
    this.getEventQueue(name).push(arg);
  }

  async stallRequest(name, options) {
    this.recordEvent(`${name}Stalled`, options);
    return new Promise(resolve => {
      this.stalledRequests[options.requestId] = resolve;
    })
  }

  /**
   * Called by the test to resume a stalled request.
   *
   * @param {number} requestId
   */
  continueRequest(requestId) {
    const continueFn = this.stalledRequests[requestId];
    if (continueFn) {
      continueFn();
    } else {
      throw new Error(`continue request: request ID not found: ${requestId}`);
    }
  }

  /**
   * Gets or creates a test event queue for a function.
   *
   * @param {string} name
   * @returns {!Queue}
   */
  getEventQueue(name) {
    if (!(name in this.eventQueues)) {
      this.eventQueues[name] = new Queue();
    }
    return this.eventQueues[name];
  }

  /**
   * Clears all the state mutated by FSP handlers (test queues, max open file
   * count).
   */
  resetState() {
    this.openedFiles = {};
    this.eventQueues = {};
    this.maxOpenedFiles = 0;
    this.stalledRequests = {};
  }

  onAbortRequested(options, onSuccess, onError) {
    this.recordEvent('onAbortRequested', options);
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
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
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.entryPath in this.files) {
      onSuccess();
      return;
    }

    onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
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
    this.recordEvent('onCloseFileRequested', options);

    if (options.fileSystemId !== this.fileSystemId ||
        !this.openedFiles[options.openRequestId]) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    delete this.openedFiles[options.openRequestId];
    onSuccess();
  };

  /**
   * FSP: implementation of copying an entry within the same file system.
   *
   * @param {!chrome.fileSystemProvider.CopyEntryRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback with an error code.
   */
  onCopyEntryRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.sourcePath === '/') {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    if (!(options.sourcePath in this.files)) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    if (options.targetPath in this.files) {
      onError(chrome.fileSystemProvider.ProviderError.EXISTS);
      return;
    }

    // Copy the metadata, but change the 'name' field.
    const source = this.files[options.sourcePath];
    /** @suppress {undefinedVars} */
    const dest = structuredClone(source);
    dest.name = options.targetPath.split('/').pop();
    this.files[options.targetPath] = dest;

    onSuccess();
  }

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
    this.recordEvent('onOpenFileRequested', options);

    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    const metadata = this.files[options.filePath].metadata;
    if (!metadata || metadata.is_directory) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    this.openedFiles[options.requestId] = options.filePath;
    this.maxOpenedFiles =
        Math.max(this.maxOpenedFiles, Object.keys(this.openedFiles).length);

    if (options.filePath === '/' + TestFileSystemProvider.FILE_STALL_OPEN) {
      this.stallRequest('onOpenFileRequested', options).then(onSuccess);
      return;
    }

    onSuccess();
  };

  /**
   * FSP: requests reading contents of a file, previously opened with <code>
   * openRequestId</code>.
   *
   * @param {!chrome.fileSystemProvider.ReadFileRequestedOptions} options
   *     Options.
   * @param {function(ArrayBuffer, boolean)} onSuccess Success callback.
   * @param {function(string)} onError Error callback.
   */
  onReadFileRequested(options, onSuccess, onError) {
    this.recordEvent('onReadFileRequested', options);

    if (options.fileSystemId !== this.fileSystemId ||
        !this.openedFiles[options.openRequestId]) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    const sendFileInChunks = (file) => {
      const array = new TextEncoder().encode(file.contents);
      const CHUNK_SIZE = 5;
      for (let i = 0; i < array.length; i += CHUNK_SIZE) {
        onSuccess(
            /*data=*/ array.slice(i, Math.min(array.length, i + CHUNK_SIZE))
                .buffer,
            /*hasMore=*/ i + CHUNK_SIZE < array.length);
      }
    };

    const filePath = this.openedFiles[options.openRequestId];
    if (filePath === '/' + TestFileSystemProvider.FILE_READ_SUCCESS) {
      sendFileInChunks(this.files[filePath]);
      return;
    }

    if (filePath === '/' + TestFileSystemProvider.FILE_FAIL) {
      onError(chrome.fileSystemProvider.ProviderError.FAILED);
      return;
    }

    if (filePath === '/' + TestFileSystemProvider.FILE_DENIED) {
      onError(chrome.fileSystemProvider.ProviderError.ACCESS_DENIED);
      return;
    }

    if (filePath === '/' + TestFileSystemProvider.FILE_BLOCKS_FOREVER) {
      // This simulates a very slow read.
      return;
    }

    if (filePath === '/' + TestFileSystemProvider.FILE_STALL_READ) {
      // Block the read until it's unblocked.
      const file = this.files[filePath];
      this.stallRequest('onReadFileRequested', options)
          .then(() => sendFileInChunks(file));
      return;
    }

    onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
  }

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
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.entryPath in this.files) {
      onSuccess();
      return;
    }

    onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
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
    this.recordEvent('onWriteFileRequested', options);

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

    if (filePath === '/' + TestFileSystemProvider.FILE_DENIED) {
      onError(chrome.fileSystemProvider.ProviderError.ACCESS_DENIED);
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
};

/**
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILESYSTEM_ID = 'test-fs';

/**
 * Reads and writes of this file always fail.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_FAIL = 'fail.txt';

/**
 * Reads and writes of this file fail with ACCESS_DENIED error.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_DENIED = 'denied.txt';

/**
 * Open requests on this file are blocked until they are manually unblocked.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_STALL_OPEN = 'stall-open.txt';

/**
 * Read requests on this file are blocked until they are manually unblocked.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_STALL_READ = 'stall-read.txt';

/**
 * Reads and writes on this file never finish.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_BLOCKS_FOREVER = 'blocks-forever.txt';

/**
 * File reads return data normally (in multiple callbacks).
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_READ_SUCCESS = 'read-normal.txt';

/**
 * Initial contents of default testing files.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.INITIAL_TEXT = 'Hello world. How are you today?';

// Service worker entry point.
export function serviceWorkerMain(serviceWorker) {
  const provider =
      new TestFileSystemProvider(TestFileSystemProvider.FILESYSTEM_ID);

  provider.setUpProviderListeners();
  provider.setUpCommandListener(serviceWorker);
}
