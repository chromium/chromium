// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function restoreEntryWorks() {
    var id = 'magic id';
    chrome.fileSystem.isRestorable(id, chrome.test.callbackPass(
        function(isRestorable) {
      chrome.test.assertTrue(isRestorable);
    }));
    chrome.fileSystem.restoreEntry(id, chrome.test.callbackPass(
        function(restoredEntry) {
      chrome.test.assertNe(null, restoredEntry);
      chrome.test.assertTrue(restoredEntry.isDirectory);
      chrome.test.assertEq(
          chrome.fileSystem.retainEntry(restoredEntry), id);
      restoredEntry.getFile(
          'writable.txt', {}, chrome.test.callback(function(entry) {
        checkEntry(entry, 'writable.txt', false /* isNew */,
                   true /*shouldBeWritable */);
      }));
    }));
  }
]);
