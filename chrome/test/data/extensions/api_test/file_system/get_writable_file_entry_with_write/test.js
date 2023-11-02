// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getWritableEntry() {
    chrome.fileSystem.chooseEntry(chrome.test.callbackPass(function(entry) {
      chrome.test.assertEq('writable.txt', entry.name);
      // Test that we can get the display path of the file.
      chrome.fileSystem.getWritableEntry(entry, chrome.test.callbackPass(
          function(writable) {
        checkEntry(writable, 'writable.txt', false, true);
      }));
    }));
  }
]);
