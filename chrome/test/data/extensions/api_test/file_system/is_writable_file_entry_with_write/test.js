// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function isWritableEntry() {
    chrome.fileSystem.chooseEntry(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // Test that the file is writable, as we have the fileSystem.write
      // permission.
      chrome.fileSystem.isWritableEntry(entry, chrome.test.callbackPass(
          function(isWritable) {
        chrome.test.assertTrue(isWritable);
      }));
    }));
  }
]);
