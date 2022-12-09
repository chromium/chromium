// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test component extension that tests fileManagerPrivate file watch api.
 * The extension adds file watch on set of entries and performs set of file
 * system operations that should trigger onDirectoryChanged events for the
 * watched entries. On file system operations is performed per a test function.
*/

/**
 * Helper class to observe the events triggered during a file system operation
 * performed during a single test function.
 * The received events are verified against the list of expected events, but
 * only after the file system operation is done. If an event is received before
 * an operation is done, it is added to the event queue that will be verified
 * after the operation. chrome.test.succeed is called when all the expected
 * events are received and verified.
 *
 */
class TestEventListener {
  constructor(id) {
    /** @type {string} */
    this.id = id;

    /**
     * Maps expectedEvent.entry.toURL() ->
     *     {expectedEvent.eventType, expectedEvent.changeType}
     *
     * Set of events that are expected to be triggered during the test. Each
     * object property represents one expected event.
     *
     * @type {Object<string, Object>}
     * @private
     */
    this.expectedEvents_ = {};

    /**
     * List of fileManagerPrivate.onDirectoryChanged events received before file
     * system operation was done.
     *
     * @type {Array<Object>}
     * @private
     */
    this.eventQueue_ = [];

    /**
     * Whether the test listener is done. When set, all further |onSuccess_| and
     * |onError| calls are ignored.
     *
     * @type {boolean}
     * @private
     */
    this.done_ = false;

    /**
     * An entry returned by the test file system operation.
     *
     * @type {Entry}
     * @private
     */
    this.receivedEntry_ = null;

    /**
     * The listener to the fileManagerPrivate.onDirectoryChanged.
     *
     * @type {function(Object)}
     * @private
     */
    this.eventListener_ = this.onDirectoryChanged_.bind(this);
  }

  /**
   * Starts listening for the onDirectoryChanged events.
   */
  start() {
    chrome.fileManagerPrivate.onDirectoryChanged.addListener(
        this.eventListener_);
  }

  /**
   * Adds expectation for an event that should be encountered during the
   * test.
   *
   * @param {Entry} entry The event's entry argument.
   */
  addExpectedEvent(entry) {
    this.expectedEvents_[entry.toURL()] = {
      eventType: 'changed',
    };
  }

  /**
   * Called by a test when the file system operation performed in the test
   * succeeds.
   *
   * @param {Entry} entry The entry returned by the file system operation.
   */
  onFileSystemOperation(entry) {
    this.receivedEntry_ = entry;
    this.eventQueue_.forEach(function(event) {
      // When done the `onError` ignores any error, so returning early here.
      if (this.done_) {
        return;
      }
      console.log('*** Checking queued events');
      this.verifyReceivedEvent_(event);
    }.bind(this));
  }

  /**
   * Called when the test encounters an error. Does cleanup and ends the
   * test with failure. Further |onError| and |onSuccess| calls will be
   * ignored.
   *
   * @param {string} message An error message.
   */
  onError(message) {
    if (this.done_) {
      return;
    }
    this.done_ = true;

    chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
        this.eventListener_);
    chrome.test.fail(message);
  }

  /**
   * Called when the test succeeds. Does cleanup and calls
   * chrome.test.succeed. Further |onError| and |onSuccess| calls will be
   * ignored.
   *
   * @private
   */
  onSuccess_() {
    if (this.done_) {
      return;
    }
    this.done_ = true;

    chrome.fileManagerPrivate.onDirectoryChanged.removeListener(
        this.eventListener_);
    chrome.test.succeed();
  }

  /**
   * onDirectoryChanged event listener.
   * If the test file system operation is done, verifies the event,
   * otherwise it adds the event to |eventQueue_|. The events from
   * |eventQueue_| will be verified once the file system operation is done.
   *
   * @param {Object} event chrome.fileManagerPrivate.onDirectoryChanged
   *     event.
   * @private
   */
  onDirectoryChanged_(event) {
    if (this.receivedEntry_) {
      this.verifyReceivedEvent_(event);
    } else {
      console.log(`*** Queued event for ${event.entry.toURL()}`);
      this.eventQueue_.push(event);
    }
  }

  /**
   * Verifies a received event.
   * It checks that there is an expected event for |event.entry.toURL()|.
   * If there is, the event is removed from the set of expected events.
   * It verifies that the received event matches the expected event
   * parameters. If the received event was the last expected event,
   * onSuccess_ is called.
   *
   * @param {Object} event chrome.fileManagerPrivate.onDirectoryChanged
   *     event.
   * @private
   */
  verifyReceivedEvent_(event) {
    const entryURL = event.entry.toURL();
    const expectedEvent = this.expectedEvents_[entryURL];

    console.log(`${this.id} verifyReceivedEvent_: ${event.eventType} ${
        event.entry.path}`);
    const state = JSON.stringify(this.expectedEvents_[entryURL]);
    console.log(`${this.id} verifyReceivedEvent_: state ${entryURL} ${state}`);

    if (!expectedEvent) {
      this.onError(
          `${this.id} Event with unexpected entryURL: ${entryURL} \n` +
          `Event type: ${event.eventType} \n`);
      return;
    }

    console.log(
        `${this.id} verifyReceivedEvent_: delete expectedEvents_ ${entryURL}`);
    delete this.expectedEvents_[entryURL];

    if (expectedEvent.eventType !== event.eventType) {
      console.log(`Marking ${this.id} as error`);
      this.onError(
          'Unexpected event type for entryURL: ' + entryURL + '\n' +
          ' Expected type: ' + expectedEvent.eventType + '\n' +
          ' Actual type: ' + event.eventType + '\n');
      return;
    }

    if (Object.keys(this.expectedEvents_).length == 0) {
      console.log(`Marking ${this.id} as success`);
      this.onSuccess_();
    }
  }
}

// Gets the path for operations. The path is relative to the mount point for
// local entries and relative to the "My Drive" root for Drive entries.
function getPath(relativePath, isOnDrive) {
  return (isOnDrive ? 'root/' : '') + relativePath;
}

/**
 * Initializes test parameters:
 * - Gets local file system.
 * - Gets the test mount point.
 * - Adds the entries that will be watched during the test.
 *
 * @param {function(Object, string)} callback The function called when the test
 *    parameters are initialized. Called with testParams object and an error
 *    message string. The error message should be ignored if testParams are
 *    valid.
 */
function initTests(callback) {
  const testParams = {
    /**
     * Whether the test parameters are valid.
     * @type {boolean}
     */
    valid: false,
    /**
     * TODO(tbarzic) : We should not need to have this. The watch api should
     * have the same behavior for local and drive file system.
     * @type {boolean}
     */
    isOnDrive: false,
    /**
     * Set of entries that are being watched during the tests.
     * @type {Object<Entry>}
     */
    entries: {},
    /**
     * File system for the testing volume.
     * @type {DOMFileSystem}
     */
    fileSystem: null
  };

  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    const possibleVolumeTypes = ['testing', 'drive'];

    const sortedVolumeMetadataList =
        volumeMetadataList
            .filter(function(volume) {
              return possibleVolumeTypes.indexOf(volume.volumeType) != -1;
            })
            .sort(function(volumeA, volumeB) {
              return possibleVolumeTypes.indexOf(volumeA.volumeType) -
                  possibleVolumeTypes.indexOf(volumeB.volumeType);
            });

    if (sortedVolumeMetadataList.length == 0) {
      callback(
          testParams, 'No volumes available, which could be used for testing.');
      return;
    }

    chrome.fileSystem.requestFileSystem(
        {volumeId: sortedVolumeMetadataList[0].volumeId, writable: true},
        function(fileSystem) {
          if (!fileSystem) {
            callback(testParams, 'Failed to acquire the testing volume.');
            return;
          }

          testParams.fileSystem = fileSystem;
          testParams.isOnDrive =
              sortedVolumeMetadataList[0].volumeType == 'drive';

          const testWatchEntries = [
            {
              name: 'file',
              path: getPath('test_dir/test_file.xul', testParams.isOnDrive),
              type: 'file'
            },
            {
              name: 'dir',
              path: getPath('test_dir/', testParams.isOnDrive),
              type: 'dir'
            },
            {
              name: 'subdir',
              path: getPath('test_dir/subdir', testParams.isOnDrive),
              type: 'dir'
            },
          ];

          // Gets the first entry in |testWatchEntries| list.
          const getNextEntry = function() {
            // If the list is empty, the test has been successfully
            // initialized, so call callback.
            if (testWatchEntries.length == 0) {
              testParams.valid = true;
              callback(testParams, 'Success.');
              return;
            }

            const testEntry = testWatchEntries.shift();

            let getFunction = null;
            if (testEntry.type == 'file') {
              getFunction = fileSystem.root.getFile.bind(fileSystem.root);
            } else {
              getFunction = fileSystem.root.getDirectory.bind(fileSystem.root);
            }

            // TODO(mtomasz): Remove this hack after migrating watchers to
            // chrome.fileSystem.
            const getFunctionAndConvert = function(path, options, callback) {
              getFunction(path, options, function(isolatedEntry) {
                chrome.fileManagerPrivate.resolveIsolatedEntries(
                    [isolatedEntry],
                    function(externalEntries) {
                      callback(externalEntries[0]);
                    });
              });
            };

            getFunctionAndConvert(testEntry.path, {},
                function(entry) {
                  testParams.entries[testEntry.name] = entry;
                  getNextEntry();
                },
                callback.bind(null, testParams,
                    'Unable to get entry: \'' + testEntry.path + '\'.'));
          };

          // Trigger getting the watched entries.
          getNextEntry();
        });
  });
};

// Starts the test.
initTests(function(testParams, errorMessage) {
  if (!testParams.valid) {
    chrome.test.notifyFail('Failed to initialize tests: ' + errorMessage);
    return;
  }

  chrome.test.runTests([
    function addFileWatch() {
      chrome.fileManagerPrivate.addFileWatch(
          testParams.entries.file,
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    function addSubdirWatch() {
      chrome.fileManagerPrivate.addFileWatch(
          testParams.entries.subdir,
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    function addDirWatch() {
      chrome.fileManagerPrivate.addFileWatch(
          testParams.entries.dir,
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    // Test that onDirectoryChanged is triggered when a directory in a watched
    // directory is created.
    function onCreateDir() {
      const testEventListener = new TestEventListener('onCreateDir');
      testEventListener.addExpectedEvent(testParams.entries.subdir);
      testEventListener.start();

      testParams.fileSystem.root.getDirectory(
          getPath('test_dir/subdir/subsubdir', testParams.isOnDrive),
          {create: true, exclusive: true},
          testEventListener.onFileSystemOperation.bind(testEventListener),
          testEventListener.onError.bind(
              testEventListener, 'Failed to create directory.'));
    },

    // Test that onDirectoryChanged is triggered when a file in a watched
    // directory is created.
    function onCreateFile() {
      const testEventListener = new TestEventListener('onCreateFile');
      testEventListener.addExpectedEvent(testParams.entries.subdir);
      testEventListener.start();

      testParams.fileSystem.root.getFile(
          getPath('test_dir/subdir/file', testParams.isOnDrive),
          {create: true, exclusive: true},
          testEventListener.onFileSystemOperation.bind(testEventListener),
          testEventListener.onError.bind(
              testEventListener, 'Failed to create file.'));
    },

    // Test that onDirectoryChanged is triggered when a file in a watched
    // directory is renamed.
    function onFileUpdated() {
      const testEventListener = new TestEventListener('onFileUpdated');
      testEventListener.addExpectedEvent(testParams.entries.subdir);

      testEventListener.start();

      testParams.fileSystem.root.getFile(
          getPath('test_dir/subdir/file', testParams.isOnDrive), {},
          function(entry) {
            entry.moveTo(
                testParams.entries.subdir, 'renamed',
                testEventListener.onFileSystemOperation.bind(testEventListener),
                testEventListener.onError.bind(
                    testEventListener, 'Failed to rename the file.'));
          },
          testEventListener.onError.bind(
              testEventListener, 'Failed to get file.'));
    },

    // Test that onDirectoryChanged is triggered when a file in a watched
    // directory is deleted.
    function onDeleteFile() {
      const testEventListener = new TestEventListener('onDeleteFile');
      testEventListener.addExpectedEvent(testParams.entries.subdir);
      testEventListener.start();

      testParams.fileSystem.root.getFile(
          getPath('test_dir/subdir/renamed', testParams.isOnDrive), {},
          function(entry) {
            entry.remove(
                testEventListener.onFileSystemOperation.bind(
                    testEventListener, entry),
                testEventListener.onError.bind(
                    testEventListener, 'Failed to remove the file.'));
          },
          testEventListener.onError.bind(
              testEventListener, 'Failed to get the file.'));
    },

    // Test that onDirectoryChanged is triggered when a watched file in a
    // watched directory is deleted.
    // The behaviour is different for drive and local mount points. On drive,
    // there will be no event for the watched file.
    function onDeleteWatchedFile() {
      const testEventListener = new TestEventListener('onDeleteWatchedFile');
      testEventListener.addExpectedEvent(testParams.entries.dir);
      testEventListener.addExpectedEvent(testParams.entries.file);
      testEventListener.start();

      testParams.fileSystem.root.getFile(
          getPath('test_dir/test_file.xul', testParams.isOnDrive), {},
          function(entry) {
            entry.remove(
                testEventListener.onFileSystemOperation.bind(
                    testEventListener, entry),
                testEventListener.onError.bind(
                    testEventListener, 'Failed to remove the file.'));
          },
          testEventListener.onError.bind(
              testEventListener, 'Failed to get the file.'));
    },

    // Test that onDirectoryChanged is triggered when a directory in a
    // watched directory is deleted.
    function onDeleteDir() {
      const testEventListener = new TestEventListener('onDeleteDir');
      testEventListener.addExpectedEvent(testParams.entries.subdir);
      testEventListener.start();

      testParams.fileSystem.root.getDirectory(
          getPath('test_dir/subdir/subsubdir', testParams.isOnDrive), {},
          function(entry) {
            entry.removeRecursively(
                testEventListener.onFileSystemOperation.bind(
                    testEventListener, entry),
                testEventListener.onError.bind(
                    testEventListener, 'Failed to remove the dir.'));
          },
          testEventListener.onError.bind(
              testEventListener, 'Failed to get the dir.'));
    },

    // Test that onDirectoryChanged is triggered when a watched directory in a
    // watched directory is deleted.
    // The behaviour is different for drive and local mount points. On drive,
    // there will be no event for the deleted directory.
    function onDeleteWatchedDir() {
      const testEventListener = new TestEventListener('onDeleteWatchedDir');
      testEventListener.addExpectedEvent(testParams.entries.subdir);
      testEventListener.addExpectedEvent(testParams.entries.dir);
      testEventListener.start();

      testParams.fileSystem.root.getDirectory(
          getPath('test_dir/subdir', testParams.isOnDrive), {},
          function(entry) {
            entry.removeRecursively(
                testEventListener.onFileSystemOperation.bind(testEventListener,
                                                             entry),
                testEventListener.onError.bind(testEventListener,
                                               'Failed to remove the dir.'));
          },
          testEventListener.onError.bind(testEventListener,
                                         'Failed to get the dir.'));
    },

    function removeFileWatch() {
      chrome.fileManagerPrivate.removeFileWatch(
          testParams.entries.file,
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    },

    function removeDirWatch() {
      chrome.fileManagerPrivate.removeFileWatch(
          testParams.entries.dir,
          chrome.test.callbackPass(function(success) {
            chrome.test.assertTrue(success);
          }));
    }

    // The watch for subdir entry is intentionally not removed to simulate the
    // case when File Manager does not remove it either (e.g. if it's opened
    // during shutdown).
  ]);
});
