// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var callbackPass = chrome.test.callbackPass;
var callbackFail = chrome.test.callbackFail;

function OpenAndRetainFile() {
  // Open a file.
  chrome.fileSystem.chooseEntry(
    { type: 'openWritableFile' },
    callbackPass(function(entry) {
      var entry_id = chrome.fileSystem.retainEntry(entry);
      chrome.test.assertNe(null, entry_id);
      chrome.test.assertTrue(entry_id.length > 0);

      // Save the file handle to local storage.
      chrome.storage.local.set(
        { file_handle: entry_id },
        callbackPass(function() {
          chrome.test.notifyPass();
        })
      );
    })
  );
}

function RestoreRetainedFile() {
  chrome.storage.local.get('file_handle', callbackPass(function(items) {
    chrome.test.assertTrue(typeof(items.file_handle) != 'undefined' &&
                           items.file_handle.length > 0);

    chrome.fileSystem.restoreEntry(
      items.file_handle,
      callbackFail('Unknown id'));
  }));
}

onload = function() {
  chrome.test.sendMessage('launched', function(reply) {
    window[reply]();
  });
};
