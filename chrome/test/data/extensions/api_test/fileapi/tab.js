// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileSystem = null;

function errorCallback(error) {
  chrome.test.fail("Got unexpected error: " + error.code);
}

function successCallback(entry) {
  chrome.test.succeed();
}

function successEntryCallback(entry) {
  fileSystem.root.getDirectory('dir', {create:false},
      function(directory) {
        // Do clean-up.  (Assume the tab won't be reloaded in testing.)
        directory.removeRecursively(successCallback, errorCallback);
      }, errorCallback);
}

chrome.test.runTests([function tab() {
  console.log("Requesting a filesystem...");
  webkitRequestFileSystem(window.TEMPORARY, 100, function(fs) {
    fileSystem = fs;
    // See if we get the same filesystem image.
    console.log("DONE requesting filesystem: " + fileSystem.name);
    fileSystem.root.getFile('dir/file', {create:false},
                            successEntryCallback, errorCallback);
  }, errorCallback);
}]);
