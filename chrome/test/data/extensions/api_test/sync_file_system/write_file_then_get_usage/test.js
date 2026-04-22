// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let fileEntry;
let fileSystem;
let usageBeforeWrite;
const TEST_DATA = '12345';
let dataSize;

const testStep = [
  function() {
    chrome.syncFileSystem.requestFileSystem(testStep.shift());
  },
  // Create empty file.
  function(fs) {
    fileSystem = fs;
    fileSystem.root.getFile(
        'Test.txt', {create: true}, testStep.shift(),
        errorHandler('getFile (create)'));
  },
  function(entry) {
    fileEntry = entry;
    testStep.shift()();
  },
  // Record usage before write.
  function() {
    chrome.syncFileSystem.getUsageAndQuota(fileSystem, testStep.shift());
  },
  function(storageInfo) {
    usageBeforeWrite = storageInfo.usageBytes;
    testStep.shift()();
  },
  // Write a known number of bytes.
  function() {
    fileEntry.createWriter(testStep.shift(), errorHandler('createWriter'));
  },
  function(fileWriter) {
    fileWriter.onwriteend = function(e) {
      testStep.shift()();
    };
    fileWriter.onerror = errorHandler('write');
    const blob = new Blob([TEST_DATA], {type: 'text/plain'});
    dataSize = blob.size;
    fileWriter.write(blob);
  },
  // Check the meta data for updated usage.
  function() {
    fileEntry.getMetadata(testStep.shift(), errorHandler('getMetadata'));
  },
  function(metadata) {
    chrome.test.assertEq(dataSize, metadata.size);
    testStep.shift()();
  },
  // Check global usage was updated.
  function() {
    chrome.syncFileSystem.getUsageAndQuota(fileSystem, testStep.shift());
  },
  function(storageInfo) {
    const usageAfterWrite = storageInfo.usageBytes;
    chrome.test.assertEq(dataSize, usageAfterWrite - usageBeforeWrite);
    chrome.test.succeed();
  },
];

function errorHandler(msg) {
  return function(error) {
    chrome.test.fail(`${msg}: ${error.name}`);
  };
}

chrome.test.runTests([testStep.shift()]);
