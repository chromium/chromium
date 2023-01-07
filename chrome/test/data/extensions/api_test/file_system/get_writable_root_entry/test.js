// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getWritableEntry() {
    chrome.fileSystem.chooseEntry(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // Test that we cannot get a writable entry for the root entry.
      chrome.fileSystem.getWritableEntry(
          entry.filesystem.root, chrome.test.callbackFail(
              'Invalid parameters'));
    }));
  }
]);
