// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mediaGalleries = chrome.mediaGalleries;

var galleries;
var testResults = [];
var expectedFileSystems;

function checkFinished() {
  if (testResults.length != galleries.length)
    return;
  var success = true;
  for (var i = 0; i < testResults.length; i++) {
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

var mediaFileSystemsDirectoryEntryCallback = function(entries) {
  testResults.push("Shouldn't have been able to get a directory listing.");
  checkFinished();
}

var mediaFileSystemsDirectoryErrorCallback = function(err) {
  testResults.push("");
  checkFinished();
};

var mediaFileSystemsListCallback = function(results) {
  galleries = results;
};

chrome.test.getConfig(function(config) {
  customArg = JSON.parse(config.customArg);
  expectedFileSystems = customArg[0];

  chrome.test.runTests([
    function getMediaFileSystems() {
      mediaGalleries.getMediaFileSystems(
          chrome.test.callbackPass(mediaFileSystemsListCallback));
    },
    function testGalleries() {
      chrome.test.assertEq(expectedFileSystems, galleries.length);
      for (var i = 0; i < galleries.length; i++) {
        var dirReader = galleries[i].root.createReader();
        dirReader.readEntries(mediaFileSystemsDirectoryEntryCallback,
                              mediaFileSystemsDirectoryErrorCallback);
      }
    },
    function validFileCopyToShouldFail() {
      runCopyToTest(validWEBPImageCase, false /* expect failure */);
    },
    function invalidFileCopyToShouldFail() {
      runCopyToTest(invalidWEBPImageCase, false /* expect failure */);
    },
  ]);
})
