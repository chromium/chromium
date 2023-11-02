// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function isNotWritableEntry() {
    chrome.fileSystem.chooseEntry(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // The file should not be writable, since we do not have the
      // fileSystem.write permission.
      chrome.fileSystem.isWritableEntry(entry, chrome.test.callbackPass(
          function(isWritable) {
        chrome.test.assertFalse(isWritable);
      }));
    }));
  }
]);
