// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let fileSystem;
const TEST_FILES = ['Test1', 'Test2'];

const testStep = [
  function() {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  // Create empty files.
  function(fs) {
    fileSystem = fs;
    createFiles(fileSystem, TEST_FILES.slice(0), testStep.shift());
  },
  // Read entries in root directory.
  function() {
    const reader = fileSystem.root.createReader();
    reader.readEntries(testStep.shift(), errorHandler);
  },
  // Query file statuses for the returned entries.
  function(entries) {
    chrome.test.assertEq(TEST_FILES.length, entries.length);
    chrome.syncFileSystem.getFileStatuses(
        entries, chrome.test.callbackPass(testStep.shift()));
  },
  // Verify the returned statuses.
  function(fileStatuses) {
    // Sort the input and results array so that their orders match.
    TEST_FILES.sort();
    fileStatuses.sort(sortByFilePath);

    chrome.test.assertEq(TEST_FILES.length, fileStatuses.length);
    for (let i = 0; i < TEST_FILES.length; ++i) {
      chrome.test.assertEq(TEST_FILES[i], fileStatuses[i].fileEntry.name);
      chrome.test.assertEq(
          `/${TEST_FILES[i]}`, fileStatuses[i].fileEntry.fullPath);
      chrome.test.assertTrue(fileStatuses[i].fileEntry.isFile);
      chrome.test.assertTrue(!fileStatuses[i].error);
      const expectedStatus = 'pending';
      chrome.test.assertEq(expectedStatus, fileStatuses[i].status);
    }
    chrome.test.succeed();
  },
];

function createFiles(fileSystem, fileNames, callback) {
  if (!fileNames.length) {
    callback();
    return;
  }
  fileSystem.root.getFile(
      fileNames.shift(), {create: true},
      createFiles.bind(null, fileSystem, fileNames, callback), errorHandler);
}

function sortByFilePath(a, b) {
  if (a.fileEntry.fullPath < b.fileEntry.fullPath) {
    return -1;
  }
  if (a.fileEntry.fullPath > b.fileEntry.fullPath) {
    return 1;
  }
  return 0;
}

function errorHandler(e) {
  console.log(`Failed test with error ${e.name}`);
  chrome.test.fail();
}

chrome.test.runTests([
  testStep.shift(),
]);
