// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(
        {suggestedName: 'open_existing.txt', acceptsMultiple: true},
        chrome.test.callbackPass(function(entries) {
          chrome.test.assertEq(1, entries.length);
          checkEntry(entries[0], 'open_existing.txt', false, false);
        })
    );
  }
]);
