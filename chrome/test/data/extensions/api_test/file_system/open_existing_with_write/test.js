// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(chrome.test.callbackPass(function(entry) {
      checkEntry(entry, 'open_existing.txt', false, true);
    }));
  }
]);
