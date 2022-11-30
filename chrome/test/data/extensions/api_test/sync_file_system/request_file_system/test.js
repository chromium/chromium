// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var syncableNameSuffix = ':Syncable';

chrome.test.runTests([
  function requestFileSystem() {
    chrome.syncFileSystem.requestFileSystem(
        chrome.test.callbackPass(function(fs) {
            chrome.test.assertTrue(fs.name != undefined);
            chrome.test.assertEq(fs.name.length - syncableNameSuffix.length,
                                 fs.name.lastIndexOf(syncableNameSuffix));
            chrome.test.assertTrue(fs.root != undefined);
            chrome.test.assertTrue(fs.root.isFile || fs.root.isDirectory);
        }));
  }
]);
