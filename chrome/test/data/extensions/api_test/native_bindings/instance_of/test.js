// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function getDisplayPath() {
    chrome.fileSystem.chooseEntry(function(entry) {
      chrome.test.assertEq('text.txt', entry.name);
      // Test that we can get the display path of the file.
      chrome.fileSystem.getDisplayPath(entry, function(path) {
        chrome.test.assertTrue(path.indexOf('native_bindings') >= 0);
        chrome.test.assertTrue(path.indexOf('text.txt') >= 0);
        chrome.test.succeed();
      });
    });
  },
]);
