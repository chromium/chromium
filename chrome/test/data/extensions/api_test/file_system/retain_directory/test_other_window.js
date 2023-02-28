// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.fileSystem.chooseEntry({type: 'openDirectory'},
    chrome.test.callbackPass(function(entry) {
  var id = chrome.fileSystem.retainEntry(entry);
  chrome.test.assertNe(null, id);
  chrome.fileSystem.isRestorable(id, chrome.test.callbackPass(
      function(isRestorable) {
    chrome.test.assertTrue(isRestorable);
  }));
  chrome.fileSystem.restoreEntry(id, chrome.test.callbackPass(
      function(restoredEntry) {
    chrome.test.assertEq(restoredEntry, entry);
  }));
  callback(id, entry);
}));
