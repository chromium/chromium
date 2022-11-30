// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This extension is a platform app that provides file handlers for .xul
 * ('xulActio') and .tiff ('tiffAction') files. It waits until the app is
 * launched with both of these handlers, and then runs tests that verify that
 * the handled entries can be successfully read.
 *
 * The events are invoked by 'file_browser/handler_test_runner' extension. Both
 * handlers will be launched once and will carry on file entry.
 */

/**
 * Attempts to read the file |entry| and verifies the file content is equal to
 * |expectedContent|.
 * If successfull, chrome.test.success is called.
 * On failure, the current test is failed.
 *
 * @param {FileEntry} entry File entry to be read.
 * @param {string} expectedContent Expected file content.
 */
function readFileAndExpectContent(entry, expectedContent) {
  chrome.test.assertFalse(!entry.file,
                          'The object does not have \'file\' method');
  entry.file(function(file) {
    var reader = new FileReader();
    reader.onloadend = function(e) {
      chrome.test.assertEq(expectedContent, reader.result);
      chrome.test.succeed();
    };
    reader.onerror = function(e) {
      chrome.test.fail('Error reading file contents.');
    };
    reader.readAsText(file);
  }, function(error) {
    chrome.test.fail('Unable to get file snapshot: ' + error.name);
  });
}

/**
 * Object that follows the app's status before chrome.test.runTests is called
 * (i.e. while the app is waiting for onLauched events).
 */
var testPreRunStatus = {
  // Whether the 'xulAction' handler has been launched.
  gotXulAction: false,
  // Whether the 'tiffAction' handler has been launched.
  gotTiffAction: false,
  // |done| is set either when the runTests is called, or an error is detected.
  // After |done| is set, all other launch event will be ignored.
  done: false,
};

/**
 * List of tests to be run for the handlers.
 *
 * @type {Array<function()>}
 */
var handlerTests = [];

/**
 * Called if an error is detected before chrome.test.runTests. It sends failure
 * notification and cancels further testing.
 *
 * @param {string} message The error message to be reported.
 */
function onError(message) {
  testPreRunStatus.done = true;
  chrome.test.notifyFail(message);
}

/**
 * Listens for onLaunched events, and runs tests once all expected events are
 * received.
 */
function launchedListener(launchData) {
  if (testPreRunStatus.done)
    return;

  if (!launchData) {
    onError('No launchData');
    return;
  }

  if (launchData.isKioskSession) {
    onError('launchData.isKioskSession incorrect.');
    return;
  }

  if (!launchData.items || launchData.items.length != 1) {
    onError('Invalid launch data items.');
    return;
  }

  if (launchData.id == 'xulAction') {
    handlerTests.push(
      function readXulAction() {
        readFileAndExpectContent(launchData.items[0].entry,
                                 'This is some test content.');
    });
    testPreRunStatus.gotXulAction = true;
  } else if (launchData.id == 'tiffAction') {
    handlerTests.push(
      function readTiffAction() {
        readFileAndExpectContent(launchData.items[0].entry,
                                 'This is some test content.');
    });
    testPreRunStatus.gotTiffAction = true;
    window.domAutomationController.send(
        `Received ${launchData.id} with: ${launchData.items[0].entry.name}`);
  } else {
    onError('Invalid handler id: \'' + launchData.id +'\'.');
    return;
  }

  if (testPreRunStatus.gotXulAction && testPreRunStatus.gotTiffAction) {
    testPreRunStatus.done = true;
    chrome.test.runTests(handlerTests);
  }
}

chrome.app.runtime.onLaunched.addListener(launchedListener);
