// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const mediaGalleries = chrome.mediaGalleries;

let galleries;
const testResults = [];
let foundGalleryWithEntry = false;
let expectedFileSystems;

function checkFinished() {
  if (testResults.length != galleries.length) {
    return;
  }
  let success = true;
  for (let i = 0; i < testResults.length; i++) {
    if (testResults[i]) {
      success = false;
    }
  }
  if (!foundGalleryWithEntry) {
    testResults.push('Did not find gallery with 1 FileEntry');
    success = false;
  }
  if (success) {
    chrome.test.succeed();
    return;
  }
  chrome.test.fail(testResults);
}

const deleteFileCallback = function(file) {
  testResults.push('');
  checkFinished();
};

const deleteFileFailedCallback = function(err) {
  testResults.push(`Couldn\'t delete file: ${err.name}`);
  checkFinished();
};

const mediaFileSystemsDirectoryEntryCallback = function(entries) {
  if (entries.length == 0) {
    testResults.push('');
  } else if (entries.length == 1) {
    if (foundGalleryWithEntry) {
      testResults.push('Found multiple galleries with 1 FileEntry');
    } else {
      foundGalleryWithEntry = true;
      entries[0].remove(deleteFileCallback, deleteFileFailedCallback);
    }
  } else {
    testResults.push('Found a gallery with more than 1 FileEntry');
  }
  checkFinished();
};

const mediaFileSystemsDirectoryErrorCallback = function(err) {
  testResults.push(`Couldn\'t read from directory: ${err.name}`);
  checkFinished();
};

const mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.getConfig(function(config) {
  const customArg = JSON.parse(config.customArg);
  expectedFileSystems = customArg[0];

  chrome.test.runTests([
    function getMediaFileSystems() {
      mediaGalleries.getMediaFileSystems(
          chrome.test.callbackPass(mediaFileSystemsListCallback));
    },
    function readFileSystemsAndDeleteFile() {
      chrome.test.assertEq(expectedFileSystems, galleries.length);
      if (expectedFileSystems == 0) {
        chrome.test.succeed();
        return;
      }

      for (let i = 0; i < galleries.length; i++) {
        const dirReader = galleries[i].root.createReader();
        dirReader.readEntries(
            mediaFileSystemsDirectoryEntryCallback,
            mediaFileSystemsDirectoryErrorCallback);
      }
    },
  ]);
});
