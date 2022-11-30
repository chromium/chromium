// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function retainEntryWorks() {
    chrome.app.window.create('test_other_window.html', chrome.test.callbackPass(
        function(otherWindow) {
      otherWindow.contentWindow.callback = chrome.test.callbackPass(
          function(id, entry) {
        otherWindow.close();
        chrome.fileSystem.isRestorable(id, chrome.test.callbackPass(
            function(isRestorable) {
          chrome.test.assertTrue(isRestorable);
        }));
        chrome.test.assertEq(chrome.fileSystem.retainEntry(entry), id);
        chrome.fileSystem.restoreEntry(id, chrome.test.callbackPass(
            function(restoredEntry) {
          chrome.test.assertEq(restoredEntry, entry);
          chrome.test.assertEq(
              chrome.fileSystem.retainEntry(restoredEntry), id);
          var reader = entry.createReader();
          reader.readEntries(chrome.test.callback(function(entries) {
            chrome.test.assertEq(entries.length, 1);
            checkEntry(entries[0], 'open_existing.txt', false, false);
          }));
          entry.getFile(
              'open_existing.txt', {},
              chrome.test.callbackPass(function(entry) {
            checkEntry(entry, 'open_existing.txt', false, false);
          }));
        }));
      });
    }));
  }
]);
