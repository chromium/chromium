// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function openDirectorySuggestedNameIgnored() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory', suggestedName: 'sub_dir'},
        chrome.test.callbackPass(function(entry) {
          chrome.test.assertNe('sub_dir', entry.name);
        }),
    );
  },
  function saveFileSuggestedNameHonored() {
    chrome.fileSystem.chooseEntry(
        {type: 'saveFile', suggestedName: 'new_file.txt'},
        chrome.test.callbackPass(function(entry) {
          chrome.test.assertEq('new_file.txt', entry.name);
        }),
    );
  },
]);
