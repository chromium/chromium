// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkTree(root, expectedEntries) {
  var directoryReader = root.createReader();
  var contents = [];
  directoryReader.readEntries(chrome.test.callbackPass(
      function readEntriesCallback(entries) {
    if (entries.length == 0) {
      chrome.test.assertEq(Object.keys(expectedEntries).length, 0);
    } else {
      for (var i = 0; i < entries.length; i++) {
        // Ignore files or directories like .svn.
        if (entries[i].name[0] == '.')
          continue;
        chrome.test.assertNe(null, expectedEntries[entries[i].name]);
        if (entries[i].isDirectory) {
          chrome.test.assertEq(typeof expectedEntries[entries[i].name],
                               'object');
          checkTree(entries[i], expectedEntries[entries[i].name]);
        } else {
          chrome.test.assertEq(expectedEntries[entries[i].name], true);
        }
        delete expectedEntries[entries[i].name];
      }
      directoryReader.readEntries(chrome.test.callbackPass(
          readEntriesCallback));
    }
  }));
}

chrome.test.getConfig(async (config) => {
  let testCases = [function getPackageDirectoryEntryCallback() {
    let expectedDirectoryEntries = {
      'manifest.json': true,
      'test': {'test.html': true, 'test.js': true}
    };
    chrome.runtime.getPackageDirectoryEntry(
        chrome.test.callbackPass(function(directoryEntry) {
          checkTree(directoryEntry, expectedDirectoryEntries);
        }));
  }];

  if (config.customArg == 'run_promise_test') {
    testCases.push(async function getPackageDirectoryEntryPromise() {
      // We have to redefine this for both tests as checkTree deletes the
      // elements as it verifies them.
      let expectedDirectoryEntries = {
        'manifest.json': true,
        'test': {'test.html': true, 'test.js': true}
      };
      let directoryEntry = await chrome.runtime.getPackageDirectoryEntry();
      checkTree(directoryEntry, expectedDirectoryEntries);
    });
  }

  chrome.test.runTests(testCases);
});
