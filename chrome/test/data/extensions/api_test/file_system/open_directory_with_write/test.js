// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing with directory access and write permission.
chrome.test.runTests([
  function moveFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.copyTo(
            directoryEntry, 'to_move.txt', chrome.test.callback(function(copy) {
          copy.moveTo(
              directoryEntry, 'moved.txt', chrome.test.callback(function(move) {
            directoryEntry.getFile(
                'to_move.txt', {}, chrome.test.callback(function(entry) {
              chrome.test.fail('Could open move source');
            }), chrome.test.callback(function() {
              checkEntry(move, 'moved.txt', false, true);
            }));
          }), chrome.test.callback(function() {
            chrome.test.fail('Failed to move file');
          }));
        }), chrome.test.callback(function() {
          chrome.test.fail('Failed to copy file');
        }));
      }), chrome.test.callback(function() {
        chrome.test.fail('Failed to open file');
      }));
    }))
  },
  function copyFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.copyTo(
            directoryEntry, 'copy.txt', chrome.test.callback(function(copy) {
          checkEntry(copy, 'copy.txt', false, true);
        }), chrome.test.callback(function() {
          chrome.test.fail('Failed to copy file');
        }));
      }), chrome.test.callback(function() {
        chrome.test.fail('Failed to open file');
      }));
    }));
  },
  function createFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'new.txt', {create: true}, chrome.test.callback(function(entry) {
        checkEntry(entry, 'new.txt', true, true);
      }), chrome.test.callback(function(err) {
        chrome.test.fail('Failed to create file');
      }));
    }));
  },
  function createDirectory() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getDirectory(
          'new', {create: true}, chrome.test.callback(function(entry) {
        chrome.test.assertEq(entry.name, 'new');
        chrome.test.succeed();
      }), chrome.test.callback(function(err) {
        chrome.test.fail('Failed to create directory');
      }));
    }));
  },
  function removeFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'remove.txt', {create: true},
          chrome.test.callback(function(entry) {
        entry.remove(chrome.test.callback(function() {
          directoryEntry.getFile(
              'remove.txt', {},
              chrome.test.callback(function() {
            chrome.test.fail('Could open deleted file');
          }), chrome.test.callback(function() {
            chrome.test.succeed();
          }))
        }), chrome.test.callback(function() {
            chrome.test.fail('Failed to delete file');
        }));
      }), chrome.test.callback(function(err) {
        chrome.test.fail('Failed to create file');
      }));
    }));
  }
]);
