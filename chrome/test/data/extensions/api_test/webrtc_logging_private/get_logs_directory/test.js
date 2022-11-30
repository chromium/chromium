// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper functions.

function readFileEntry(fileEntry, callback) {
  fileEntry.file(
      function(file) {
        var fileReader = new FileReader();
        fileReader.onload = function(e) {
          callback(e.target.result);
        };
        fileReader.onerror = function(error) {
          chrome.test.fail('Failed to read file: ' + error);
        };
        fileReader.readAsText(file);
      },
      function(error) {
        chrome.test.fail('Failed to get file from entry: ' + error);
      });
}

function readFileEntries(directoryEntry, callback) {
  var reader = directoryEntry.createReader();
  reader.readEntries(callback, function(error) {
    chrome.test.fail('Failed to readEntries: ' + error);
  });
}

// Tests.

function testGetsReadOnlyDirectoryEntry() {
  // Looks and quacks like a DirectoryEntry, since that type is implemented as
  // [NoInterfaceObject].
  // See:
  // https://groups.google.com/a/chromium.org/forum/#!topic/blink-dev/_6Euwqv366U
  chrome.webrtcLoggingPrivate.getLogsDirectory(function(directoryEntry) {
    chrome.test.assertTrue(!!directoryEntry);
    chrome.test.assertTrue(directoryEntry.isDirectory);
    chrome.test.assertEq('WebRTC Logs', directoryEntry.name);
    directoryEntry.getFile(
        'new.file', {create: true, exclusive: true},
        function() {
          chrome.test.fail('DirectoryEntry allowed file creation');
        }, chrome.test.succeed);
  });
}

function testEmptyDirectory() {
  chrome.webrtcLoggingPrivate.getLogsDirectory(function(directoryEntry) {
    readFileEntries(directoryEntry, function(entries) {
      chrome.test.assertEq(0, entries.length);
      chrome.test.succeed();
    });
  });
}

function testCanReadFile() {
  chrome.webrtcLoggingPrivate.getLogsDirectory(function(directoryEntry) {
    readFileEntries(directoryEntry, function(entries) {
      chrome.test.assertEq(1, entries.length);
      var fileEntry = entries[0];
      chrome.test.assertTrue(fileEntry.isFile);
      chrome.test.assertEq('test.file', fileEntry.name);
      readFileEntry(fileEntry, function(contents) {
        chrome.test.assertEq('test file contents', contents);
        chrome.test.succeed();
      });
    });
  });
}

function testCanRemoveFile() {
  chrome.webrtcLoggingPrivate.getLogsDirectory(function(directoryEntry) {
    readFileEntries(directoryEntry, function(entries) {
      chrome.test.assertEq(1, entries.length);
      var fileEntry = entries[0];
      chrome.test.assertTrue(fileEntry.isFile);
      fileEntry.remove(function() {
        readFileEntries(directoryEntry, function(entries) {
          chrome.test.assertEq(0, entries.length);
          chrome.test.succeed();
        });
      });
    });
  });
}

function testCannotWriteToFile() {
  chrome.webrtcLoggingPrivate.getLogsDirectory(function(directoryEntry) {
    readFileEntries(directoryEntry, function(entries) {
      chrome.test.assertEq(1, entries.length);
      var fileEntry = entries[0];
      fileEntry.createWriter(function(fileWriter) {
        function expectErrorSet() {
          chrome.test.assertTrue(!!fileWriter.error);
          chrome.test.succeed();
        };
        var data = new Blob(['example'], {type: 'text/plain'});
        fileWriter.onprogress = expectErrorSet;
        fileWriter.onerror = expectErrorSet;
        fileWriter.writeEnd = expectErrorSet;
        fileWriter.write(data);
      }, chrome.test.succeed);
    });
  });
}

var testGroups = {
  'test_without_directory': [
    testGetsReadOnlyDirectoryEntry,
    testEmptyDirectory,
  ],
  'test_with_file_in_directory': [
    testGetsReadOnlyDirectoryEntry,
    testCanReadFile,
    testCannotWriteToFile,
    // Must be run last, since it deletes the test file.
    testCanRemoveFile,
  ],
};

chrome.test.getConfig(function(config) {
  var testGroupName = config.customArg;
  if (!testGroupName) {
    chrome.test.fail('No tests specified');
    return;
  }
  var tests = testGroups[testGroupName];
  if (!tests) {
    chrome.test.fail('No tests found with name=' + testGroupName);
    return;
  }
  chrome.test.runTests(tests);
});
