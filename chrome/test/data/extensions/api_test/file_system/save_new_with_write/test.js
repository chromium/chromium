// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function saveFile() {
    chrome.fileSystem.chooseFile({type: 'saveFile'},
        chrome.test.callbackPass(function(entry) {
      checkEntry(entry, 'save_new.txt', true, true);
    }));
  }
]);
