// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(
        {suggestedName: '%.txt'},
        chrome.test.callbackPass(function(entry) {
          checkEntry(entry, '_.txt', false, false);
        })
    );
  }
]);
