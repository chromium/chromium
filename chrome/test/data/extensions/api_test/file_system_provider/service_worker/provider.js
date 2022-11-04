// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {promisifyWithLastError, Queue} from '/_test_resources/api_test/file_system_provider/service_worker/helpers.js';

/**
 * Splits the path into dir name and base name, e.g. '/a/b/c' -> '/a/b', 'c'.
 *
 * @param {string} pathString
 * @returns {{dirPath: string, fileName:string}}
 */
function splitPath(pathString) {
  const path = pathString.split('/');
  const fileName = path.pop();
  return {dirPath: path.join('/'), fileName};
}

/**
 * @param {string} text
 * @returns {!ArrayBuffer}
 */
function textToBuffer(text) {
  return new TextEncoder().encode(text).buffer;
}

class Entry {
  /**
   * @param {string} key name identifying the entry in a directory (separate
   *    from name in the metadata to allow returning a different value there).
   * @param {{
   *  name: string,
   *  isDirectory: boolean,
   *  size: (number|undefined),
   *  modificationTime: !Date
   * }} metadata
   * @param {?string} contents
   * @param {Array<!Entry>} children
   */
  constructor(key, metadata, contents, children) {
    this.key = key;
    this.metadata = metadata;
    this.contents = contents;
    /** @type {!Object<string, !Entry>} */
    this.children = Object.fromEntries((children || []).map(e => [e.key, e]));
  }

  /**
   * @param {string} name
   * @param {!Date} modificationTime
   * @param {string} contents
   */
  static file(name, modificationTime, contents) {
    return new Entry(
        name, {
          name,
          isDirectory: false,
          size: contents.length,
          modificationTime,
        },
        contents, null);
  }

  /**
   * @param {string} name
   * @param {!Date} modificationTime
   * @param {!Array<!Entry>} children
   */
  static dir(name, modificationTime, children) {
    return new Entry(
        name, {
          name,
          size: 0,
          isDirectory: true,
          modificationTime,
        },
        null, children);
  }
};

export class TestFileSystemProvider {
  constructor(fileSystemId) {
    this.fileSystemId = fileSystemId;
    /**
     * Filesystem contents (data and metadata). The key is a full path, and the
     * value is an object containing metadata and file contents.
     *
     * @private {!Entry}
     */
    this.root = Entry.dir('', new Date(2014, 4, 28, 10, 39, 15), [
      // Directory with actions.
      Entry.dir(
          TestFileSystemProvider.DIR_WITH_ACTIONS,
          new Date(2014, 1, 25, 7, 36, 12), []),
      // Directory with no actions.
      Entry.dir(
          TestFileSystemProvider.DIR_WITH_NO_ACTIONS,
          new Date(2014, 1, 25, 7, 36, 12), []),
      // Read error
      Entry.file(
          TestFileSystemProvider.FILE_FAIL, new Date(2014, 1, 25, 7, 36, 12),
          TestFileSystemProvider.INITIAL_TEXT),
      // Open blocks until unblocked manually.
      Entry.file(
          TestFileSystemProvider.FILE_BLOCK_OPEN,
          new Date(2014, 1, 26, 8, 37, 13),
          TestFileSystemProvider.INITIAL_TEXT),
      // Read blocks until unblocked manually.
      Entry.file(
          TestFileSystemProvider.FILE_BLOCK_IO,
          new Date(2014, 1, 26, 8, 37, 13),
          TestFileSystemProvider.INITIAL_TEXT),
      // Read returns data in chunks.
      Entry.file(
          TestFileSystemProvider.FILE_READ_SUCCESS,
          new Date(2014, 1, 25, 7, 36, 12),
          TestFileSystemProvider.INITIAL_TEXT),
      // A big file to test access at offset greater than the max unsigned
      // 32-bit value.
      (() => {
        const entry = Entry.file(
            TestFileSystemProvider.FILE_BIG, new Date(2014, 1, 25, 7, 36, 12),
            '');
        entry.metadata.size = 6 * 1024 * 1024 * 1024;
        return entry;
      })(),
      // Read returns more data than asked for.
      Entry.file(
          TestFileSystemProvider.FILE_TOO_LARGE_CHUNK,
          new Date(2014, 1, 25, 7, 36, 12), 'A'.repeat(1024 * 2)),
      // Read handlers invokes both success and error callbacks.
      Entry.file(
          TestFileSystemProvider.FILE_INVALID_CALLBACK,
          new Date(2014, 1, 25, 7, 36, 12), 'A'.repeat(1024 * 2)),
      // File with negative size.
      (() => {
        const entry = Entry.file(
            TestFileSystemProvider.FILE_NEGATIVE_SIZE,
            new Date(2014, 1, 25, 7, 36, 12), 'A'.repeat(1024 * 2));
        entry.metadata.size = -entry.metadata.size;
        return entry;
      })(),
      // File with invalid date for the modification time.
      Entry.file(
          TestFileSystemProvider.FILE_INVALID_DATE, new Date('Invalid date.'),
          ''),
      // File with only isDirectory flag as metadata.
      (() => {
        const entry = Entry.file(
            TestFileSystemProvider.FILE_ONLY_TYPE,
            new Date(2014, 1, 25, 7, 36, 12), '');
        const {isDirectory} = entry.metadata;
        entry.metadata = {isDirectory};
        return entry;
      })(),
      // File with only size as metadata.
      (() => {
        const entry = Entry.file(
            TestFileSystemProvider.FILE_ONLY_TYPE_AND_SIZE,
            new Date(2014, 1, 25, 7, 36, 12), 'A'.repeat(1024 * 4));
        const {isDirectory, size} = entry.metadata;
        entry.metadata = {isDirectory, size};
        return entry;
      })(),
    ]);

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
     * A queue of recorded event per event name. Allows the remote test to read
     * the events happening in the FSP implementation asynchronously.
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

    /**
     * Configuration set by tests.
     *
     * @private {!Object<string, ?>}
     */
    this.testConfig = {};
  }

  setUpProviderListeners() {
    this.setHandlerEnabled('onAbortRequested', true);
    this.setHandlerEnabled('onAddWatcherRequested', true);
    this.setHandlerEnabled('onCloseFileRequested', true);
    this.setHandlerEnabled('onConfigureRequested', true);
    this.setHandlerEnabled('onCopyEntryRequested', true);
    this.setHandlerEnabled('onCreateDirectoryRequested', true);
    this.setHandlerEnabled('onCreateFileRequested', true);
    this.setHandlerEnabled('onDeleteEntryRequested', true);
    this.setHandlerEnabled('onExecuteActionRequested', true);
    this.setHandlerEnabled('onGetActionsRequested', true);
    this.setHandlerEnabled('onGetMetadataRequested', true);
    this.setHandlerEnabled('onMountRequested', true);
    this.setHandlerEnabled('onMoveEntryRequested', true);
    this.setHandlerEnabled('onOpenFileRequested', true);
    this.setHandlerEnabled('onReadDirectoryRequested', true);
    this.setHandlerEnabled('onReadFileRequested', true);
    this.setHandlerEnabled('onRemoveWatcherRequested', true);
    this.setHandlerEnabled('onTruncateRequested', true);
    this.setHandlerEnabled('onUnmountRequested', true);
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

  setUpCommandListener() {
    const listener = (msg, sender, sendResponse) => {
      const {commandId, args} = msg;
      this.handleCommand(commandId, ...args).then(sendResponse);
      return true;  // Indicate that we want to respond asynchronously.
    };
    // Listen to both events to handle messages sent both from the same or from
    // a different extension.
    chrome.runtime.onMessageExternal.addListener(listener);
    chrome.runtime.onMessage.addListener(listener);
  }

  /** @suppress {checkTypes} */
  async handleCommand(commandId, ...args) {
    const result = {};
    try {
      if (commandId in this) {
        result.response = await this[commandId](...args);
      } else {
        result.error = `unhandled: ${commandId}`;
      }
    } catch (error) {
      result.error = error.toString();
    }
    return result;
  }

  /**
   * Called by the test. Add files to the provider's filesystem.
   *
   * @param {!Object<string, !Entry>} files a map of paths to entries.
   */
  addFiles(files) {
    for (const path of Object.keys(files)) {
      const file = files[path];
      const {dirPath} = splitPath(path);
      // Restore Date objects after receiving data via postMessage.
      file.metadata.modificationTime = new Date(file.metadata.modificationTime);
      const entry =
          new Entry(file.metadata.name, file.metadata, file.contents, null);
      this.findEntryByPath(dirPath).children[entry.metadata.name] = entry;
    }
  }

  /**
   * Called by the test. Gets contents of a given file.
   *
   * @param {string} filePath
   * @returns {string|null} The current text contents of the file.
   */
  getFileContents(filePath) {
    const entry = this.findEntryByPath(filePath);
    return entry ? entry.contents : null;
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
   * Called by the test. Causes a change notification to be sent for an entry.
   *
   * @param {string} entryPath
   * @param {boolean} recursive
   * @param {string} tag
   */
  async triggerNotify(entryPath, recursive, tag) {
    return promisifyWithLastError(chrome.fileSystemProvider.notify, {
      fileSystemId: this.fileSystemId,
      observedPath: entryPath,
      recursive,
      changeType: 'CHANGED',
      tag,
    });
  }

  /**
   * Opens a tab from the extension hosting this provider.
   *
   * @param {string} url
   * @returns {!Promise<number>}
   */
  async openTab(url) {
    const tab = await promisifyWithLastError(chrome.tabs.create, {url});
    return tab.id;
  }

  /**
   * Closes a tab previously opened with |openTab|.
   *
   * @param {number} tabId
   */
  async closeTab(tabId) {
    await chrome.tabs.remove(tabId);
  }

  /**
   * Opens a window from the extension hosting this provider.
   *
   * @param {string} url
   * @returns {!Promise<number>}
   */
  async openWindow(url) {
    return await promisifyWithLastError(chrome.windows.create, {url});
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
   * Called by the test. Gets the count of events for an event name, to check
   * that there's been no events without blocking. When using, ensure that "no
   * event" condition is gated by some other condition you can wait for (e.g. a
   * request failing and returning a result).
   *
   * @param {string} eventName
   * @returns {number}
   */
  getEventCount(eventName) {
    return this.getEventQueue(eventName).size();
  }

  /**
   * Called by the tests to control provider configuration for different test
   * scenarios.
   * @param {string} key
   * @param {?} value
   */
  setConfig(key, value) {
    if (value === undefined || value === null) {
      delete this.testConfig[key];
    } else {
      this.testConfig[key] = value;
    }
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
   * Clears all the state mutated by tests or FSP handlers.
   */
  resetState() {
    this.openedFiles = {};
    this.eventQueues = {};
    this.maxOpenedFiles = 0;
    this.stalledRequests = {};
    this.testConfig = {};
  }

  /**
   * Finds a file or directory entry by path.
   *
   * @param {string} pathString
   * @returns {?Entry}
   */
  findEntryByPath(pathString) {
    if (pathString === '/') {
      return this.root;
    }
    let path = pathString.split('/');
    if (path[0] != '') {
      // Must start with "/"
      return null;
    }
    path = path.slice(1);

    let entry = this.root;
    for (const fileName of path) {
      const child = entry.children[fileName];
      if (child) {
        entry = child;
      } else {
        return null;
      }
    }
    return entry;
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

    if (options.entryPath === '/' + TestFileSystemProvider.FILE_FAIL) {
      onError(chrome.fileSystemProvider.ProviderError.FAILED);
      return;
    }

    if (this.findEntryByPath(options.entryPath)) {
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
   *
   * @param {!chrome.fileSystemProvider.ConfigureRequestedOptions} options
   * @param {function()} onSuccess
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback.
   */
  onConfigureRequested(options, onSuccess, onError) {
    this.recordEvent('onConfigureRequested', options);
    const delay = this.testConfig['onConfigureRequestedDelayMs'];
    if (delay) {
      // Simulates a delayed configuration success.
      setTimeout(onSuccess, delay);
      return;
    }
    const error = this.testConfig['onConfigureRequestedError'];
    if (error) {
      onError(error);
    } else {
      onSuccess();
    }
  }

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

    const source = this.findEntryByPath(options.sourcePath);
    if (!source) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    if (this.findEntryByPath(options.targetPath)) {
      onError(chrome.fileSystemProvider.ProviderError.EXISTS);
      return;
    }

    // Copy the metadata, but change the 'name' field.
    /** @suppress {undefinedVars} */
    const dest = structuredClone(source);
    const {dirPath, fileName} = splitPath(options.targetPath);
    dest.name = fileName;
    this.findEntryByPath(dirPath).children[dest.name] = dest;

    onSuccess();
  }

  /**
   * FSP: implementation of creating a directory within the same file system.
   *
   * @param {!chrome.fileSystemProvider.CreateDirectoryRequestedOptions} options
   *  Options.
   * @param {function()} onSuccess Success callback
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *  callback with an error code.
   */
  onCreateDirectoryRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.directoryPath === '/' || options.recursive) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    if (this.findEntryByPath(options.directoryPath)) {
      onError(chrome.fileSystemProvider.ProviderError.EXISTS);
      return;
    }

    // Check new directory path is valid.
    const dirPathSplit = splitPath(options.directoryPath);
    const parentDir = this.findEntryByPath(dirPathSplit.dirPath);
    const dirName = dirPathSplit.fileName;
    if (!parentDir) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }
    if (!parentDir.metadata.isDirectory) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_A_DIRECTORY);
      return;
    }

    // Add new directory.
    const emptyDirMetadata = {
      isDirectory: true,
      name: dirName,
      modificationTime: new Date(2014, 4, 28, 10, 39, 15),
    }

    const entry = new Entry(dirName, emptyDirMetadata, null, null);
    parentDir.children[dirName] = entry;

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

    if (this.findEntryByPath(options.filePath)) {
      onError(chrome.fileSystemProvider.ProviderError.EXISTS);
      return;
    }

    const {dirPath, fileName} = splitPath(options.filePath);
    const dir = this.findEntryByPath(dirPath);
    if (!dir) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    dir.children[fileName] = Entry.file(fileName, new Date(), '');
    onSuccess();
  };

  /**
   * FSP: implementation for the execute action request event.
   *
   * @param {!chrome.fileSystemProvider.ExecuteActionRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback with an error code.
   */
  onExecuteActionRequested(options, onSuccess, onError) {
    this.recordEvent('onExecuteActionRequested', options);
    if (options.actionId === TestFileSystemProvider.ACTION_ID) {
      onSuccess();
    } else {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
    }
  }

  /**
   * FSP: implementation for returning a list of actions for the requested
   * entry.
   *
   * @param {chrome.fileSystemProvider.GetActionsRequestedOptions} options
   *     Options.
   * @param {function(Array<Object>)} onSuccess Success callback with a list of
   *     actions.
   * @param {function(string)} onError Error callback with an error code.
   */
  onGetActionsRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.entryPaths.indexOf(
            '/' + TestFileSystemProvider.DIR_WITH_NO_ACTIONS) !== -1) {
      onSuccess([]);
      return;
    }

    if (options.entryPaths.indexOf(
            '/' + TestFileSystemProvider.DIR_WITH_ACTIONS) !== -1) {
      onSuccess(TestFileSystemProvider.ACTIONS);
      return;
    }

    onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
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
    this.recordEvent('onGetMetadataRequested', options);
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    const entry = this.findEntryByPath(options.entryPath);
    if (!entry) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    // Returning a thumbnail while not requested is not allowed for performance
    // reasons. Remove the field if needed. However, do not remove it for one
    // file, to simulate an error.
    if (!options.thumbnail && entry.metadata.thumbnail &&
        options.entryPath !==
            `/${TestFileSystemProvider.FILE_ALWAYS_VALID_THUMBNAIL}`) {
      const metadataWithoutThumbnail = {
        ...entry.metadata,
        thumbnail: undefined,
      };
      onSuccess(metadataWithoutThumbnail);
      return;
    }
    onSuccess(entry.metadata);
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

    const entry = this.findEntryByPath(options.filePath);
    if (!entry || entry.metadata.isDirectory) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    this.openedFiles[options.requestId] = options.filePath;
    this.maxOpenedFiles =
        Math.max(this.maxOpenedFiles, Object.keys(this.openedFiles).length);

    if (options.filePath === '/' + TestFileSystemProvider.FILE_BLOCK_OPEN) {
      this.stallRequest('onOpenFileRequested', options).then(onSuccess);
      return;
    }

    onSuccess();
  };

  /**
   * FSP: implementation of moving an entry within the same file system.
   *
   * @param {!chrome.fileSystemProvider.MoveEntryRequestedOptions} options
   *  Options.
   * @param {function()} onSuccess Success callback
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *  callback with an error code.
   */
  onMoveEntryRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.sourcePath === '/') {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const source = this.findEntryByPath(options.sourcePath);
    if (!source) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    if (this.findEntryByPath(options.targetPath)) {
      onError(chrome.fileSystemProvider.ProviderError.EXISTS);
      return;
    }

    // Move the metadata with changing the 'name' field.
    let {dirPath, fileName} = splitPath(options.targetPath);
    const dir = this.findEntryByPath(dirPath);
    if (!dir) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }
    if (!dir.metadata.isDirectory) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_A_DIRECTORY);
      return;
    }
    source.metadata.name = fileName;
    dir.children[fileName] = source;

    // Remove the source file.
    ({dirPath, fileName} = splitPath(options.sourcePath));
    delete this.findEntryByPath(dirPath).children[fileName];

    onSuccess();
  }

  /**
   * Returns entries in the requested directory.
   *
   * @param {!chrome.fileSystemProvider.ReadDirectoryRequestedOptions} options
   *     Options.
   * @param {function(Array<Object>, boolean)} onSuccess Success callback with
   *     a list of entries. May be called multiple times.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback with an error code.
   */
  onReadDirectoryRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    const entry = this.findEntryByPath(options.directoryPath);
    if (!entry.metadata.isDirectory) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    const children = Object.values(entry.children);
    // Send one-by-one to have multiple result callbacks.
    for (let i = 0; i < children.length; i++) {
      onSuccess(
          [children[i].metadata],
          /*hasMore=*/ i < children.length - 1);
    }
  }


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

    const filePath = this.openedFiles[options.openRequestId];
    const entry = this.findEntryByPath(filePath);
    if (!entry) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const sendFileInChunks = (file) => {
      const buffer = textToBuffer(file.contents);
      const CHUNK_SIZE = 5;
      for (let i = 0; i < buffer.byteLength; i += CHUNK_SIZE) {
        onSuccess(
            /*data=*/ buffer.slice(
                i, Math.min(buffer.byteLength, i + CHUNK_SIZE)),
            /*hasMore=*/ i + CHUNK_SIZE < buffer.byteLength);
      }
    };

    if (filePath === '/' + TestFileSystemProvider.FILE_TOO_LARGE_CHUNK) {
      // Invalid file: returns more data than the file size.
      const buffer = textToBuffer('A'.repeat(entry.metadata.size * 4));
      onSuccess(buffer, /*hasMore=*/ true);
      onSuccess(buffer, /*hasMore=*/ true);
      onSuccess(buffer, /*hasMore=*/ true);
      onSuccess(buffer, /*hasMore=*/ false);
      return;
    }

    if (filePath === '/' + TestFileSystemProvider.FILE_INVALID_CALLBACK) {
      // Invalid file: invokes both success and error callbacks.
      const buffer = textToBuffer('A'.repeat(options.length));
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      onSuccess(buffer, /*hasMore=*/ false);
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

    if (filePath === '/' + TestFileSystemProvider.FILE_BLOCK_IO) {
      // Block the read until it's unblocked.
      this.stallRequest('onReadFileRequested', options)
          .then(() => sendFileInChunks(entry));
      return;
    }

    if (filePath == '/' + TestFileSystemProvider.FILE_BIG) {
      // This file is not intended to be read below the max 32-bit unsigned
      // value, so fail immediately.
      if (options.offset <= 2 ** 32 - 1) {
        onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
        return;
      }
      // The return value does not matter, so just return a string of "A"s.
      // Encoded length is the same as string length for ASCII.
      onSuccess(
          /*data=*/ textToBuffer('A'.repeat(options.length)),
          /*hasMore=*/ false,
      );
      return;
    }

    sendFileInChunks(entry);
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

    const error = this.testConfig['onRemoveWatcherRequestedError'];
    if (error) {
      onError(error);
      return;
    }

    if (!this.findEntryByPath(options.entryPath)) {
      onSuccess();
      return;
    }

    onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
  };

  /**
   * FSP: implementation for truncating a file to the specified length.
   *
   * @param {!chrome.fileSystemProvider.TruncateRequestedOptions} options
   *     Options.
   * @param {function()} onSuccess Success callback.
   * @param {function(string)} onError Error callback.
   */
  onTruncateRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    let entry = this.findEntryByPath(options.filePath);
    if (!entry) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    // Truncating beyond the end of the file.
    if (options.length > entry.metadata.size) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    entry.metadata.size = options.length;
    onSuccess();
  }


  /**
   * FSP: requests to mount this filesystem.
   *
   * @param {function()} onSuccess Success callback.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback.
   */
  onMountRequested(onSuccess, onError) {
    // This handler does not take the options arguments.
    this.recordEvent('onMountRequested', {});
    onSuccess();
  };

  /**
   * FSP: requests to unmount this filesystem.
   *
   * @param {!chrome.fileSystemProvider.UnmountRequestedOptions} options
   * @param {function()} onSuccess Success callback.
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *     callback.
   */
  onUnmountRequested(options, onSuccess, onError) {
    this.recordEvent('onUnmountRequested', options);
    const error = this.testConfig['onUnmountRequestedError'];
    if (error) {
      onError(error);
    } else {
      onSuccess();
    }
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
    const entry = this.findEntryByPath(filePath);
    if (!entry) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const metadata = entry.metadata;

    if (metadata.isDirectory) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
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

    // Writing beyond the end of the file.
    if (options.offset > metadata.size) {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const continueWrite = () => {
      // Create an array with enough space for new data.
      const prevContents = textToBuffer(entry.contents || '');
      const newLength = Math.max(
          prevContents.byteLength, options.offset + options.data.byteLength);
      const newContents = new Uint8Array(new ArrayBuffer(newLength));
      // Write existing data and new data.
      newContents.set(new Uint8Array(prevContents), 0);
      newContents.set(new Uint8Array(options.data), options.offset);
      // Save the new file as text.
      entry.contents = new TextDecoder().decode(newContents);
      metadata.size = newContents.length;
      onSuccess();
    };

    if (filePath === '/' + TestFileSystemProvider.FILE_BLOCK_IO) {
      // Block the write until it's unblocked.
      this.stallRequest('onWriteFileRequested', options).then(continueWrite);
      return;
    }

    continueWrite();
  }

  /**
   * FSP: implementation of deleting an entry within the same file system.
   *
   * @param {!chrome.fileSystemProvider.DeleteEntryRequestedOptions} options
   *  Options.
   * @param {function()} onSuccess Success callback
   * @param {function(chrome.fileSystemProvider.ProviderError)} onError Error
   *  callback with an error code.
   */
  onDeleteEntryRequested(options, onSuccess, onError) {
    if (options.fileSystemId !== this.fileSystemId) {
      onError(chrome.fileSystemProvider.ProviderError.SECURITY);
      return;
    }

    if (options.entryPath === '/') {
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const entry = this.findEntryByPath(options.entryPath);
    if (!entry) {
      onError(chrome.fileSystemProvider.ProviderError.NOT_FOUND);
      return;
    }

    if (entry.metadata.isDirectory &&
        (!options.recursive && Object.keys(entry.children).length > 0)) {
      // Don't allow non-recursive deletion of non-empty directories.
      onError(chrome.fileSystemProvider.ProviderError.INVALID_OPERATION);
      return;
    }

    const {dirPath, fileName} = splitPath(options.entryPath);
    const parentEntry = this.findEntryByPath(dirPath);
    delete parentEntry.children[fileName];

    onSuccess();
  }
};

/**
 * @type {string}
 * @const
 */
TestFileSystemProvider.DIR_WITH_ACTIONS = 'actions';

/**
 * @type {string}
 * @const
 */
TestFileSystemProvider.DIR_WITH_NO_ACTIONS = 'no-actions';

/**
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILESYSTEM_ID = 'test-fs';

/**
 * Reads, writes and add/remove watchers of this file always fail.
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
TestFileSystemProvider.FILE_BLOCK_OPEN = 'block-open.txt';

/**
 * Read and write requests on this file are blocked until they are manually
 * unblocked.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_BLOCK_IO = 'block-io.txt';

/**
 * File reads return data normally (in multiple callbacks).
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_READ_SUCCESS = 'read-normal.txt';

/**
 * A file bigger than the max unsigned 32-bit value.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_BIG = 'read-big.txt';

/**
 * File read requests return more data than asked for.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_TOO_LARGE_CHUNK = 'read-too-large-chunks.txt';

/**
 * File read requests call both error and success callbacks.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_INVALID_CALLBACK = 'read-invalid-callback.txt';

/**
 * File with negative size in metadata.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_NEGATIVE_SIZE = 'negative-size.txt';

/**
 * File with invalid modification time.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_INVALID_DATE = 'invalid-date.txt';

/**
 * File with only type for metadata fields.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_ONLY_TYPE = 'metadata-only-type.txt';

/**
 * File with only size for metadata fields.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_ONLY_TYPE_AND_SIZE =
    'metadata-only-type-and-size.txt';

/**
 * File always with valid thumbnail.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.FILE_ALWAYS_VALID_THUMBNAIL =
    'always-with-thumbnail.txt';

/**
 * Initial contents of default testing files.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.INITIAL_TEXT = 'Hello world. How are you today?';

/**
 * Valid thumbnail for testing files.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.VALID_THUMBNAIL =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAUA' +
    'AAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO' +
    '9TXL0Y4OHwAAAABJRU5ErkJggg==';

/**
 * A valid action ID that will execute successfully.
 *
 * @type {string}
 * @const
 */
TestFileSystemProvider.ACTION_ID = 'test-action-id';

/**
 * @type {Array<Object>}
 * @const
 */
TestFileSystemProvider.ACTIONS = Object.freeze(
    [{id: 'SHARE'}, {id: 'SomeCustomAction', title: 'Do something custom'}]);

// Service worker entry point.
export function serviceWorkerMain(serviceWorker) {
  const provider =
      new TestFileSystemProvider(TestFileSystemProvider.FILESYSTEM_ID);

  provider.setUpProviderListeners();
  provider.setUpCommandListener();
}
