// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function readOnlyVolume() {
    // Requesting a writable access to a read-only volume is incorrect and
    // shoult result in an error.
    chrome.fileSystem.requestFileSystem(
        {volumeId: 'testing:read-only', writable: true},
        chrome.test.callbackFail('Security error.', function(fs) {
        }));
  },
  function writableVolume() {
    chrome.fileSystem.requestFileSystem(
        {volumeId: 'testing:writable', writable: true},
        chrome.test.callbackPass(function(fileSystem) {
          chrome.test.assertFalse(!!chrome.runtime.lastError);
          chrome.test.assertTrue(!!fileSystem);
        }));
  },
  // Verify that it's impossible to get a writable access to a file system which
  // was requested without "write: true" option. Otherwise, users would see the
  // dialog for granting read-only access, but the access would be R/W.
  function writableRootOnlyViaRequestFileSystem() {
    chrome.fileSystem.requestFileSystem(
        {volumeId: 'testing:writable'},
        chrome.test.callbackPass(function(fileSystem) {
          chrome.test.assertFalse(!!chrome.runtime.lastError);
          chrome.test.assertTrue(!!fileSystem);
          // Accessing a R/W root must fail.
          chrome.fileSystem.getWritableEntry(
              fileSystem.root,
              chrome.test.callbackFail(
                  'Invalid parameters', function(writableRootEntry) {}));
          // Accessing a child directory must fail too.
          fileSystem.root.getDirectory('child-dir', {create: false},
              chrome.test.callbackPass(function(childEntry) {
                chrome.fileSystem.getWritableEntry(
                    fileSystem.root,
                    chrome.test.callbackFail(
                        'Invalid parameters',
                        function(writableChildEntry) {}));
              }), function(error) {
                chrome.test.fail(error.name);
              });
        }));
  }
]);
