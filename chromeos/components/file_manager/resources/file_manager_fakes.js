// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {checkTypes} */
window.chrome.extension = {
  inIncognitoContext: false,
};

/** @suppress {checkTypes} */
window.chrome.storage = {
  onChanged: {
    addListener() {},
  },
  sync: {
    /** @private {!Object} store */
    store_: {},
    /**
     * @param {!Array<string>|string} keys
     * @param {function((!Object)} callback
     */
    get: (keys, callback) => {
      const inKeys = Array.isArray(keys) ? keys : [keys];
      const result = {};
      inKeys.forEach(key => {
        if (key in chrome.storage.sync.store_) {
          result[key] = chrome.storage.sync.store_[key];
        }
      });
      setTimeout(callback, 0, result);
    },
    /**
     * @param {!Object<string>} items
     * @param {function()=} opt_callback
     */
    set: (items, opt_callback) => {
      for (const key in items) {
        chrome.storage.sync.store_[key] = items[key];
      }
      if (opt_callback) {
        setTimeout(opt_callback);
      }
    },
  },
  local: {
    /** @private {!Object} store */
    store_: {},
    /**
     * @param {!Array<string>|string} keys
     * @param {function((!Object)} callback
     */
    get: (keys, callback) => {
      const inKeys = Array.isArray(keys) ? keys : [keys];
      const result = {};
      inKeys.forEach(key => {
        if (key in chrome.storage.local.store_) {
          result[key] = chrome.storage.local.store_[key];
        }
      });
      setTimeout(callback, 0, result);
    },
    /**
     * @param {!Object<string>} items
     * @param {function()=} opt_callback
     */
    set: (items, opt_callback) => {
      for (const key in items) {
        chrome.storage.local.store_[key] = items[key];
      }
      if (opt_callback) {
        setTimeout(opt_callback);
      }
    },
  },
};

window.BackgroundWindowSWA = class {
  constructor() {
    /**
     * @type {!FileBrowserBackground|!FileBrowserBackgroundFull}
     */
    this.background = new FileBrowserBackgroundFull();

    /**
     * @type {!Object}
     */
    this.launcher = {
      /** @param {Object=} appState App state. */
      launchFileManager(appState) {
        window.fileManagerLaunchNewWindow(appState);
      },
    };

    /**
     * @type {!DriveSyncHandler}
     */
    this.driveSyncHandler = new DriveSyncHandler();
  }

  /**
   * @param {Window} window
   */
  registerDialog(window) {}
}

window.FileBrowserBackground = class {
  constructor() {
    /**
     * Dialogs
     * @type {!Object<!Window>}
     */
    this.dialogs;

    /**
     * String assets. Notable difference here: files app extern defines
     * this string object on FileBrowserBackgroundFull.
     * @type {Object<string>}
     */
    this.stringData;
  }

  /**
   * @param {function()} callback Ready callback.
   */
  ready(callback) {
    const script = document.createElement('script');

    script.onload = () => {
      this.stringData = window.loadTimeData.data_;
      window.loadTimeData.data_ = null;
      callback();
    };

    document.head.append(script);
    script.src = 'strings.js';
  }
}

window.FileBrowserBackgroundFull = class extends FileBrowserBackground {
  constructor() {
    super();

    /**
     * @type {!DriveSyncHandler}
     */
    this.driveSyncHandler;

    /**
     * @type {!ProgressCenter}
     */
    this.progressCenter = new ProgressCenter();

    /**
     * @type {FileOperationManager}
     */
    this.fileOperationManager = new FileOperationManager();

    /**
     * @type {!importer.ImportRunner}
     */
    this.mediaImportHandler = new MediaImportHandler();

    /**
     * @type {!importer.MediaScanner}
     */
    this.mediaScanner = new MediaScanner();

    /**
     * @type {!importer.HistoryLoader}
     */
    this.historyLoader = new HistoryLoader();

    /**
     * @type {!Crostini}
     */
    this.crostini = new Crostini();

    /**
     * @private @type {VolumeManager}
     */
    this.volumeManagerInstance_;
  }

  /**
   * Returns VolumeManager instance.
   * @public
   * @return {!VolumeManager}
   */
  getVolumeManager() {
    if (!this.volumeManagerInstance_) {
      this.volumeManagerInstance_ = new VolumeManager();
    }

    return this.volumeManagerInstance_;
  }
}

window.VolumeManager = class {
  constructor() {
    /**
     * The list of VolumeInfo instances for each mounted volume.
     * @type {VolumeInfoList}
     */
    this.volumeInfoList = new VolumeInfoListFake();
  }

  /**
   * Disposes the instance. After the invocation of this method, any other
   * method should not be called.
   */
  dispose() {}

  /**
   * Obtains a volume info containing the passed entry.
   * @param {!Entry|!FilesAppEntry} entry Entry on the volume to be
   *     returned. Can be fake.
   * @return {?VolumeInfo} The VolumeInfo instance or null if not found.
   */
  getVolumeInfo(entry) {}

  /**
   * Returns the drive connection state.
   * @return {chrome.fileManagerPrivate.DriveConnectionState} Connection state.
   */
  getDriveConnectionState() {
    return {type: 'FAKE-DRIVE-CONNECTION-TYPE'};
  }

  /**
   * @param {string} fileUrl File url to the archive file.
   * @param {string=} password Password to decrypt archive file.
   * @return {!Promise<!VolumeInfo>} Fulfilled on success, otherwise rejected
   *     with a VolumeManagerCommon.VolumeError.
   */
  mountArchive(fileUrl, password) {}

  /**
   * Unmounts a volume.
   * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
   * @return {!Promise<void>} Fulfilled on success, otherwise rejected with a
   *     VolumeManagerCommon.VolumeError.
   */
  unmount(volumeInfo) {}

  /**
   * Configures a volume.
   * @param {!VolumeInfo} volumeInfo Volume to be configured.
   * @return {!Promise} Fulfilled on success, otherwise rejected with an error
   *     message.
   */
  configure(volumeInfo) {}

  /**
   * Obtains volume information of the current profile.
   *
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
   * @return {VolumeInfo} Volume info.
   */
  getCurrentProfileVolumeInfo(volumeType) {}

  /**
   * Obtains location information from an entry.
   *
   * @param {!Entry|!FilesAppEntry} entry File or directory entry. It
   *     can be a fake entry.
   * @return {?EntryLocation} Location information.
   */
  getLocationInfo(entry) {}

  /**
   * Adds an event listener to the target.
   * @param {string} type The name of the event.
   * @param {function(!Event)} handler The handler for the event. This is
   *     called when the event is dispatched.
   */
  addEventListener(type, handler) {}

  /**
   * Removes an event listener from the target.
   * @param {string} type The name of the event.
   * @param {function(!Event)} handler The handler for the event.
   */
  removeEventListener(type, handler) {}

  /**
   * Dispatches an event and calls all the listeners that are listening to
   * the type of the event.
   * @param {!Event} event The event to dispatch.
   * @return {boolean} Whether the default action was prevented. If someone
   *     calls preventDefault on the event object then this returns false.
   */
  dispatchEvent(event) {}

  /**
   * Searches the information of the volume that exists on the given device
   * path.
   * @param {string} devicePath Path of the device to search.
   * @return {VolumeInfo} The volume's information, or null if not found.
   */
  findByDevicePath(devicePath) {}

  /**
   * Returns a promise that will be resolved when volume info, identified
   * by {@code volumeId} is created.
   *
   * @param {string} volumeId
   * @return {!Promise<!VolumeInfo>} The VolumeInfo. Will not resolve
   *     if the volume is never mounted.
   */
  whenVolumeInfoReady(volumeId) {}

  /**
   * Obtains the default display root entry.
   * @param {function(DirectoryEntry)|function(FilesAppDirEntry)} callback
   * Callback passed the default display root.
   */
  getDefaultDisplayRoot(callback) {
    setTimeout(callback);
  }
}

window.VolumeInfoListFake = class {
  constructor() {
    /**
     * Holds VolumeInfo instances.
     * @private @const {cr.ui.ArrayDataModel}
     */
    this.model_ = new cr.ui.ArrayDataModel([]);
    Object.freeze(this);
  }

  get length() {
    return this.model_.length;
  }

  /** @override */
  addEventListener(type, handler) {
    this.model_.addEventListener(type, handler);
  }

  /** @override */
  removeEventListener(type, handler) {
    this.model_.removeEventListener(type, handler);
  }

  /** @override */
  add(volumeInfo) {
    const index = this.findIndex(volumeInfo.volumeId);
    if (index !== -1) {
      this.model_.splice(index, 1, volumeInfo);
    } else {
      this.model_.push(volumeInfo);
    }
  }

  /** @override */
  remove(volumeId) {
    const index = this.findIndex(volumeId);
    if (index !== -1) {
      this.model_.splice(index, 1);
    }
  }

  /** @override */
  item(index) {
    return /** @type {!VolumeInfo} */ (this.model_.item(index));
  }

  /**
   * Obtains an index from the volume ID.
   * @param {string} volumeId Volume ID.
   * @return {number} Index of the volume.
   */
  findIndex(volumeId) {
    for (let i = 0; i < this.model_.length; i++) {
      if (this.model_.item(i).volumeId === volumeId) {
        return i;
      }
    }
    return -1;
  }
}

window.DriveSyncHandler = class extends EventTarget {
  /**
   * Returns the completed event name.
   * @return {string}
   */
  getCompletedEventName() {}

  /**
   * Returns whether the Drive sync is currently suppressed or not.
   * @return {boolean}
   */
  isSyncSuppressed() {}

  /**
   * Shows a notification that Drive sync is disabled on cellular networks.
   */
  showDisabledMobileSyncNotification() {}

  /**
   * @return {boolean} Whether the handler is syncing items or not.
   */
  get syncing() {}
}

window.Crostini = class {
  /**
   * Initialize enabled settings.
   * Must be done after loadTimeData is available.
   */
  initEnabled() {}

  /**
   * Initialize Volume Manager.
   * @param {!VolumeManager} volumeManager
   */
  initVolumeManager(volumeManager) {}

  /**
   * Register for any shared path changes.
   */
  listen() {}

  /**
   * Set whether the specified VM is enabled.
   * @param {string} vmName
   * @param {boolean} enabled
   */
  setEnabled(vmName, enabled) {}

  /**
   * Returns true if the specified VM is enabled.
   * @param {string} vmName
   * @return {boolean}
   */
  isEnabled(vmName) {}

  /**
   * Registers an entry as a shared path for the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  registerSharedPath(vmName, entry) {}

  /**
   * Unregisters entry as a shared path from the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  unregisterSharedPath(vmName, entry) {}

  /**
   * Returns true if entry is shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @return {boolean} True if path is shared either by a direct
   *   share or from one of its ancestor directories.
   */
  isPathShared(vmName, entry) {}

  /**
   * Returns true if entry can be shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @param {boolean} persist If path is to be persisted.
   */
  canSharePath(vmName, entry, persist) {}
}

window.ProgressCenter = class {
  /**
   * Turns off sending updates when a file operation reaches 'completed' state.
   * Used for testing UI that can be ephemeral otherwise.
   */
  neverNotifyCompleted() {}
  /**
   * Updates the item in the progress center.
   * If the item has a new ID, the item is added to the item list.
   * @param {ProgressCenterItem} item Updated item.
   */
  updateItem(item) {}

  /**
   * Requests to cancel the progress item.
   * @param {string} id Progress ID to be requested to cancel.
   */
  requestCancel(id) {}

  /**
   * Adds a panel UI to the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  addPanel(panel) {}

  /**
   * Removes a panel UI from the notification center.
   * @param {ProgressCenterPanelInterface} panel Panel UI.
   */
  removePanel(panel) {}

  /**
   * Obtains item by ID.
   * @param {string} id ID of progress item.
   * @return {?ProgressCenterItem} Progress center item having the specified
   *     ID. Null if the item is not found.
   */
  getItemById(id) {}
}

window.FileOperationManager = class extends EventTarget {
  /**
   * Says if there are any tasks in the queue.
   * @return {boolean} True, if there are any tasks.
   */
  hasQueuedTasks() {}

  /**
   * Requests the specified task to be canceled.
   * @param {string} taskId ID of task to be canceled.
   */
  requestTaskCancel(taskId) {}

  /**
   * Filters the entry in the same directory
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry|FakeEntry} targetEntry The destination entry of the
   *     target directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @return {Promise} Promise fulfilled with the filtered entry. This is not
   *     rejected.
   */
  filterSameDirectoryEntry(sourceEntries, targetEntry, isMove) {}

  /**
   * Kick off pasting.
   *
   * @param {Array<Entry>} sourceEntries Entries of the source files.
   * @param {DirectoryEntry} targetEntry The destination entry of the target
   *     directory.
   * @param {boolean} isMove True if the operation is "move", otherwise (i.e.
   *     if the operation is "copy") false.
   * @param {string=} opt_taskId If the corresponding item has already created
   *     at another places, we need to specify the ID of the item. If the
   *     item is not created, FileOperationManager generates new ID.
   */
  paste(sourceEntries, targetEntry, isMove, opt_taskId) {}

  /**
   * Returns true if all entries will use trash for delete.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries.
   * @return {boolean}
   */
  willUseTrash(volumeManager, entries) {}

  /**
   * Schedules the files deletion.
   *
   * @param {!Array<!Entry>} entries The entries.
   */
  deleteEntries(entries) {}

  /**
   * Restores files from trash.
   *
   * @param {Array<!{name: string, filesEntry: !Entry, infoEntry: !FileEntry}>}
   *     trashEntries The trash entries.
   */
  restoreDeleted(trashEntries) {}

  /**
   * Creates a zip file for the selection of files.
   *
   * @param {!Array<!Entry>} selectionEntries The selected entries.
   * @param {!DirectoryEntry} dirEntry The directory containing the selection.
   */
  zipSelection(selectionEntries, dirEntry) {}

  /**
   * Generates new task ID.
   *
   * @return {string} New task ID.
   */
  generateTaskId() {}
}

window.ImportHistory = class {
  /**
   * @return {!Promise<!importer.ImportHistory>} Resolves when history
   *     has been fully loaded.
   */
  whenReady() {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously copied to the device.
   */
  wasCopied(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<boolean>} Resolves with true if the FileEntry
   *     was previously imported to the specified destination.
   */
  wasImported(entry, destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @param {string} destinationUrl
   */
  markCopied(entry, destination, destinationUrl) {}

  /**
   * List urls of all files that are marked as copied, but not marked as synced.
   * @param {!importer.Destination} destination
   * @return {!Promise<!Array<string>>}
   */
  listUnimportedUrls(destination) {}

  /**
   * @param {!FileEntry} entry
   * @param {!importer.Destination} destination
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImported(entry, destination) {}

  /**
   * @param {string} destinationUrl
   * @return {!Promise<?>} Resolves when the operation is completed.
   */
  markImportedByUrl(destinationUrl) {}

  /**
   * Adds an observer, which will be notified when cloud import history changes.
   *
   * @param {!importer.ImportHistory.Observer} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importer.ImportHistory.Observer} observer
   */
  removeObserver(observer) {}
}

window.MediaScanner = class {
  /**
   * Initiates scanning.
   *
   * @param {!DirectoryEntry} directory
   * @param {!importer.ScanMode} mode
   * @return {!importer.ScanResult} ScanResult object representing the scan
   *     job both while in-progress and when completed.
   */
  scanDirectory(directory, mode) {}

  /**
   * Initiates scanning.
   *
   * @param {!Array<!FileEntry>} entries Must be non-empty, and all entries
   *     must be of a supported media type. Individually supplied files
   *     are not subject to deduplication.
   * @param {!importer.ScanMode} mode The method to detect new files.
   * @return {!importer.ScanResult} ScanResult object representing the scan
   *     job for the explicitly supplied entries.
   */
  scanFiles(entries, mode) {}

  /**
   * Adds an observer, which will be notified on scan events.
   *
   * @param {!importer.ScanObserver} observer
   */
  addObserver(observer) {}

  /**
   * Remove a previously registered observer.
   *
   * @param {!importer.ScanObserver} observer
   */
  removeObserver(observer) {}
}

/**
 * Class representing the results of an {importer} scan operation.
 *
 * @interface
 */
window.ScanResult = class {
  /**
   * @return {boolean} true if scanning is complete.
   */
  isFinal() {}

  /**
   * Notifies the scan to stop working. Some in progress work
   * may continue, but no new work will be undertaken.
   */
  cancel() {}

  /**
   * @return {boolean} True if the scan has been canceled. Some
   * work started prior to cancellation may still be ongoing.
   */
  canceled() {}

  /**
   * @param {number} count Sets the total number of candidate entries
   *     that were checked while scanning. Used when determining
   *     total progress.
   */
  setCandidateCount(count) {}

  /**
   * Event method called when a candidate has been processed.
   * @param {number} count
   */
  onCandidatesProcessed(count) {}

  /**
   * Returns all files entries discovered so far. The list will be
   * complete only after scanning has completed and {@code isFinal}
   * returns {@code true}.
   *
   * @return {!Array<!FileEntry>}
   */
  getFileEntries() {}

  /**
   * Returns all files entry duplicates discovered so far.
   * The list will be
   * complete only after scanning has completed and {@code isFinal}
   * returns {@code true}.
   *
   * Duplicates are files that were found during scanning,
   * where not found in import history, and were matched to
   * an existing entry either in the import destination, or
   * to another entry within the scan itself.
   *
   * @return {!Array<!FileEntry>}
   */
  getDuplicateFileEntries() {}

  /**
   * Returns a promise that fires when scanning is finished
   * normally or has been canceled.
   *
   * @return {!Promise<!importer.ScanResult>}
   */
  whenFinal() {}

  /**
   * @return {!importer.ScanResult.Statistics}
   */
  getStatistics() {}
}

window.MediaImportHandler = class {
  /**
   * @param {!ProgressCenter} progressCenter
   * @param {!importer.HistoryLoader} historyLoader
   * @param {!importer.DispositionChecker.CheckerFunction} dispositionChecker
   * @param {!DriveSyncHandler} driveSyncHandler
   */
  constructor(
      progressCenter, historyLoader, dispositionChecker, driveSyncHandler) {}

  importFromScanResult(scanResult, destination, directoryPromise) {}
}

/**
 * Provider of lazy loaded importer.ImportHistory. This is the main
 * access point for a fully prepared {@code importer.ImportHistory} object.
 */
window.HistoryLoader = class {
  /**
   * Instantiates an {@code importer.ImportHistory} object and manages any
   * necessary ongoing maintenance of the object with respect to
   * its external dependencies.
   *
   * @see importer.SynchronizedHistoryLoader for an example.
   *
   * @return {!Promise<!importer.ImportHistory>} Resolves when history instance
   *     is ready.
   */
  getHistory() {}

  /**
   * Adds a listener to be notified when history is fully loaded for the first
   * time. If history is already loaded...will be called immediately.
   *
   * @param {function(!importer.ImportHistory)} listener
   */
  addHistoryLoadedListener(listener) {}
}
