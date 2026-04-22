// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const mediaGalleries = chrome.mediaGalleries;

let galleries;
const testResults = [];
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
  if (success) {
    chrome.test.succeed();
    return;
  }
  chrome.test.fail(testResults);
}

const mediaFileSystemsDirectoryEntryCallback = function(entries) {
  testResults.push(`Shouldn't have been able to get a directory listing.`);
  checkFinished();
};

const mediaFileSystemsDirectoryErrorCallback = function(err) {
  testResults.push('');
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
    function testGalleries() {
      chrome.test.assertEq(expectedFileSystems, galleries.length);
      for (let i = 0; i < galleries.length; i++) {
        const dirReader = galleries[i].root.createReader();
        dirReader.readEntries(
            mediaFileSystemsDirectoryEntryCallback,
            mediaFileSystemsDirectoryErrorCallback);
      }
    },
    function validFileCopyToShouldFail() {
      runCopyToTest(VALID_WEBP_IMAGE_CASE, false /* expect failure */);
    },
    function invalidFileCopyToShouldFail() {
      runCopyToTest(INVALID_WEBP_IMAGE_CASE, false /* expect failure */);
    },
  ]);
});
