// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Testing with directory access, but no write permission.
chrome.test.runTests([
  function openFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {},
          chrome.test.callback(function(entry) {
        checkEntry(entry, 'open_existing.txt', false, false);
      }));
    }));
  },
  function readDirectory() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      var reader = directoryEntry.createReader();
      reader.readEntries(chrome.test.callback(function(entries) {
        // On POSIX systems DIR_HOME is overridden for this test and
        // dotfiles may be created there, ignore them
        // See https://codereview.chromium.org/200473002/ and
        // https://crrev.com/c/2858114.
        var testEntry;
        entries.forEach(function(entry) {
          if (!entry.name.startsWith('.')) {
            chrome.test.assertEq(entry.name, 'open_existing.txt');
            testEntry = entry;
          }
        });
        checkEntry(testEntry, 'open_existing.txt', false, false);
      }));
    }));
  },
  function removeFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.remove(chrome.test.callback(function() {
          chrome.test.fail('Could delete a file without permission');
        }), chrome.test.callback(function() {
          chrome.test.succeed();
        }));
      }));
    }));
  },
  function copyFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.copyTo(
            directoryEntry, 'copy.txt', chrome.test.callback(function() {
          chrome.test.fail('Could copy a file without permission.');
        }), chrome.test.callback(function() {
          chrome.test.succeed();
        }));
      }));
    }));
  },
  function moveFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'open_existing.txt', {}, chrome.test.callback(function(entry) {
        entry.moveTo(
            directoryEntry, 'moved.txt', chrome.test.callback(function() {
          chrome.test.fail('Could move a file without permission.');
        }), chrome.test.callback(function() {
          chrome.test.succeed();
        }));
      }));
    }));
  },
  function createFile() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getFile(
          'new.txt', {create: true}, chrome.test.callback(function() {
        chrome.test.fail('Could create a file without permission.');
      }), chrome.test.callback(function() {
        chrome.test.succeed();
      }));
    }));
  },
  function createDirectory() {
    chrome.fileSystem.chooseEntry(
        {type: 'openDirectory'},
        chrome.test.callbackPass(function(directoryEntry) {
      directoryEntry.getDirectory(
          'new', {create: true}, chrome.test.callback(function() {
        chrome.test.fail('Could create a directory without permission.');
      }), chrome.test.callback(function() {
        chrome.test.succeed();
      }));
    }));
  }
]);
