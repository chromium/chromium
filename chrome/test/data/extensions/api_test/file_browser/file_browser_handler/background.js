// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This extension provides two file browser handlers: 'ReadOnly' and
 * 'ReadWrite'. 'ReadOnly' handler handles .xul files and has read-only file
 * access. 'ReadWrite' handler handles .tiff files and has read-write file
 * access.
 *
 * The extension waits until both handlers are executed and then runs tests for
 * them. The tests verify that the extension is able to read/write the handled
 * files as defined by the handlers' file access permissions. If there is an
 * error before the tests are run, chrome.test.notifyFail will be called, and
 * further onExecute events will be ignored.
 *
 * The handlers are executed by 'file_browser/handler_test_runner' extension.
 * Each of 'ReadOnly' and 'ReadWrite' handlers should be executed once, and each
 * should carry exactly one handled file.
 */

// Initial content of handled files. The content is set in
// external_filesystem_apitest.cc.
var kInitialTestFileContent = 'This is some test content.';
// Content written by write test.
var kTextToWrite = ' Yay!';

/**
 * Asserts that |value| equals |expectedValue|. If the assert fails, current
 * test fails with |errorMessage|. Otherwise, |callback()| is called.
 */
function assertEqAndRunCallback(expectedValue, value, errorMessage, callback) {
  chrome.test.assertEq(expectedValue, value, errorMessage);
  callback();
}

/**
 * Attempts to read file and asserts that the read success and file content are
 * as expected (|expectSuccess|, |expectedContent|). On success |callback| is
 * called, otherwise current test is failed.
 *
 * @param {FileEntry} entry Entry to be read.
 * @param {boolean} expectSuccess Whether the read should succeed.
 * @param {string} expectedContent If the read succeeds, the expected content to
 *     be read from file. If the read fails, it is ignored.
 * @param {function()} callback Function called if the read ends as defined by
 *     |expectSuccess| and |expectedContent|.
 */
function readAndExpectContent(entry, expectSuccess, expectedContent, callback) {
  var error = 'Reading file \'' + entry.fullPath + '\'.';
  var reader = new FileReader();

  reader.onload = function() {
    chrome.test.assertTrue(expectSuccess, error);
    assertEqAndRunCallback(expectedContent, reader.result, error, callback);
  };

  entry.file(reader.readAsText.bind(reader),
             assertEqAndRunCallback.bind(null,
                 false, expectSuccess, error, callback));
};

/**
 * Attempts to write |content| to the end of the |entry| and verifies that the
 * operation success is as expected. On success |callback| is called, else the
 * current test is failed.
 *
 * @param {FileEntry} entry File entry to be read.
 * @param {string} content String content to be appended to the file.
 * @param {boolean} expectSuccess Whether the write should succeed.
 * @param {function()} callback Function called if write ends as defined by
 *     |expectSuccess|.
 */
function write(entry, content, expectSuccess, callback) {
  var error = 'Writing to: \'' + entry.fullPath + '\'.';

  entry.createWriter(function(writer) {
    writer.onerror = assertEqAndRunCallback.bind(null, expectSuccess, false,
                                                 error, callback);
    writer.onwrite = assertEqAndRunCallback.bind(null, expectSuccess, true,
                                                 error, callback);

    writer.seek(kInitialTestFileContent.length);
    var blob = new Blob([kTextToWrite], {type: 'text/plain'});
    writer.write(blob);
  },
  assertEqAndRunCallback.bind(null, expectSuccess, false,
      'Getting writer for: \'' + entry.fullPath + '\'.', callback));
};

/**
 * Runs read test.
 *
 * @params {FileEntry} entry File entry for which the test should be run.
 * @params {boolean} expectSuccess Whether the read should succeed.
 */
function readTest(entry, expectSuccess) {
  readAndExpectContent(entry, expectSuccess, kInitialTestFileContent,
                       chrome.test.succeed)
}

/**
 * Runs test for a file that is not executed for any handler. Test tries to get
 * an existing file entry from the |entry|'s filesystem. The file path is
 * constructed by appending '.foo' to the |entry|'s path (the Chrome part of the
 * test should ensure that the file exists). The get operation is expected to
 * fail.
 */
function getSiblingTest(entry) {
  var error = 'Got file (\'' + entry.fullPath.concat('.foo') + '\') for which' +
              'file access was not granted.';
  entry.filesystem.root.getFile(entry.fullPath.concat('.foo'), {},
                                function(entry) { chrome.test.fail(error); },
                                chrome.test.succeed);
}

/**
 * Runs write test.
 * Attempts to write to the entry. If the write operation ends as expected, the
 * test verifies new content of the file.
 *
 * @param {FileEntry} entry Entry to be written.
 * @param {boolean} expectSuccess Whether the test should succeed.
 */
function writeTest(entry, expectSuccess) {
  var verifyFileContent = function() {
    var expectedContent = kInitialTestFileContent;
    // The test file content should change only if the write operatino
    // succeeded.
    if (expectSuccess)
      expectedContent = expectedContent.concat(kTextToWrite);

    readAndExpectContent(entry, true, expectedContent, chrome.test.succeed);
  };

  write(entry, kTextToWrite, expectSuccess, verifyFileContent);
}

/**
 * Object that follows the extensions's status before chrome.test.runTests is
 * called (i.e. while it's waiting for onExecute events).
 */
var testPreRunStatus = {
  // Whether the 'ReadOnly' handler has been executed.
  gotReadOnlyAction: false,
  // Whether the 'ReadWrite' handler has been executed.
  gotReadWriteAction: false,
  // |done| is set either when the runTests is called, or an error is detected.
  // After |done| is set, all other onExecute events will be ignored.
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
 * Listens for onExecute events, and runs tests once all expected events are
 * received.
 */
function onExecuteListener(id, details) {
  if (testPreRunStatus.done)
    return;

  var fileEntries = details.entries;
  if (!fileEntries || fileEntries.length != 1) {
    onError('Unexpected file entries size.');
    return;
  }

  if ((id == 'ReadOnly' && testPreRunStatus.gotReadOnlyAction) ||
      (id == 'ReadWrite' && testPreRunStatus.gotReadWriteAction)) {
    onError('Action \'' + id + '\' executed more than once.');
    return;
  }

  if (id == 'ReadOnly') {
    var entry = fileEntries[0];

    // Add tests for read-only handler.
    handlerTests.push(function readReadOnly() { readTest(entry, true); });
    handlerTests.push(
        function getSiblingReadOnly() { getSiblingTest(entry); });
    handlerTests.push(function writeReadOnly() { writeTest(entry, false); });

    testPreRunStatus.gotReadOnlyAction = true;
  } else if (id == 'ReadWrite') {
    var entry = fileEntries[0];

    // Add tests for read-write handler.
    handlerTests.push(function readReadWrite() { readTest(entry, true); });
    handlerTests.push(
        function getSilblingReadWrite() { getSiblingTest(entry); });
    handlerTests.push(function writeReadWrite() { writeTest(entry, true); });

    testPreRunStatus.gotReadWriteAction = true;
  } else {
    onError('Unexpected action id: ' + id);
    return;
  }

  if (testPreRunStatus.gotReadOnlyAction &&
      testPreRunStatus.gotReadWriteAction) {
    testPreRunStatus.done = true;
    chrome.test.runTests(handlerTests);
  }
}

chrome.fileBrowserHandler.onExecute.addListener(onExecuteListener);
