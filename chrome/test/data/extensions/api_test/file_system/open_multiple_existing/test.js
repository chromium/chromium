// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(
        {acceptsMultiple: true},
        chrome.test.callbackPass(function(entries) {
          chrome.test.assertEq(2, entries.length);
          // Ensure entry names are in sort order: crbug.com/1103147
          if (entries[0].name === 'open_existing2.txt') {
            entries = [entries[1], entries[0]];
          }
          checkEntry(entries[0], 'open_existing1.txt', false, false);
          checkEntry(entries[1], 'open_existing2.txt', false, false);
        })
    );
  }
]);
