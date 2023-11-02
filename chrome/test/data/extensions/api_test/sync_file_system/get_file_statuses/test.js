// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileSystem;
var testFiles = ['Test1', 'Test2'];

var testStep = [
  function() {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  // Create empty files.
  function(fs) {
    fileSystem = fs;
    createFiles(fileSystem, testFiles.slice(0), testStep.shift());
  },
  // Read entries in root directory.
  function() {
    var reader = fileSystem.root.createReader();
    reader.readEntries(testStep.shift(), errorHandler);
  },
  // Query file statuses for the returned entries.
  function(entries) {
    chrome.test.assertEq(testFiles.length, entries.length);
    chrome.syncFileSystem.getFileStatuses(
      entries, chrome.test.callbackPass(testStep.shift()));
  },
  // Verify the returned statuses.
  function(fileStatuses) {
    // Sort the input and results array so that their orders match.
    testFiles.sort();
    fileStatuses.sort(sortByFilePath);

    chrome.test.assertEq(testFiles.length, fileStatuses.length);
    for (var i = 0; i < testFiles.length; ++i) {
      chrome.test.assertEq(testFiles[i], fileStatuses[i].fileEntry.name);
      chrome.test.assertEq('/' + testFiles[i],
                           fileStatuses[i].fileEntry.fullPath);
      chrome.test.assertTrue(fileStatuses[i].fileEntry.isFile);
      chrome.test.assertTrue(!fileStatuses[i].error);
      var expectedStatus = 'pending';
      chrome.test.assertEq(expectedStatus, fileStatuses[i].status);
    }
    chrome.test.succeed();
  }
];

function createFiles(fileSystem, fileNames, callback) {
  if (!fileNames.length) {
    callback();
    return;
  }
  fileSystem.root.getFile(
    fileNames.shift(), {create: true},
    createFiles.bind(null, fileSystem, fileNames, callback),
    errorHandler);
}

function sortByFilePath(a, b) {
  if (a.fileEntry.fullPath < b.fileEntry.fullPath)
    return -1;
  if (a.fileEntry.fullPath > b.fileEntry.fullPath)
    return 1;
  return 0;
}

function errorHandler(e) {
  console.log("Failed test with error" + e.name);
  chrome.test.fail();
}

chrome.test.runTests([
  testStep.shift()
]);
