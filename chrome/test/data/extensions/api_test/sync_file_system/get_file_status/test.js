// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testStep = [
  function() {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  // Create empty file.
  function(fileSystem) {
    fileSystem.root.getFile('Test.txt', {create: true}, testStep.shift(),
        errorHandler);
  },
  // Confirm file is pending as this is the mocked value.
  function(fileEntry) {
    chrome.syncFileSystem.getFileStatus(fileEntry, testStep.shift());
  },
  function(fileStatus) {
    chrome.test.assertEq("pending", fileStatus);
    chrome.test.succeed();
  }
];

function errorHandler(e) {
  console.log("Failed test with error" + e.name);
  chrome.test.fail();
}

chrome.test.runTests([
  testStep.shift()
]);
