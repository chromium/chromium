// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

expectedDirectoryEntries = {
  'background.js': true,
  'manifest.json': true,
  'window': {
    'test.html': true,
    'test.js': true
  }
};

function checkTree(root, expectedEntries) {
  var directoryReader = root.createReader();
  var contents = [];
  directoryReader.readEntries(chrome.test.callbackPass(
      function readEntriesCallback(entries) {
    if (entries.length == 0) {
      chrome.test.assertEq(Object.keys(expectedEntries).length, 0);
    } else {
      for (var i = 0; i < entries.length; i++) {
        // Ignore files or directories like .svn.
        if (entries[i].name[0] == '.')
          continue;
        chrome.test.assertNe(null, expectedEntries[entries[i].name]);
        if (entries[i].isDirectory) {
          chrome.test.assertEq(typeof expectedEntries[entries[i].name],
                               'object');
          checkTree(entries[i], expectedEntries[entries[i].name]);
        } else {
          chrome.test.assertEq(expectedEntries[entries[i].name], true);
          chrome.fileSystem.isWritableEntry(
              entries[i], chrome.test.callbackPass(function(isWritable) {
            chrome.test.assertFalse(isWritable);
          }));
          chrome.fileSystem.getWritableEntry(
              entries[i], chrome.test.callbackFail(
                  'Invalid parameters'));
        }
        delete expectedEntries[entries[i].name];
      }
      directoryReader.readEntries(chrome.test.callbackPass(
          readEntriesCallback));
    }
  }));
}

chrome.test.runTests([
  function getPackageDirectoryEntry() {
    chrome.runtime.getPackageDirectoryEntry(chrome.test.callbackPass(
        function(directoryEntry) {
      checkTree(directoryEntry, expectedDirectoryEntries);
    }));
  }
]);
