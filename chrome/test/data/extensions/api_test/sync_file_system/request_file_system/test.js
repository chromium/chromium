// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const SYNCABLE_NAME_SUFFIX = ':Syncable';

chrome.test.runTests([
  function requestFileSystem() {
    chrome.syncFileSystem.requestFileSystem(
        chrome.test.callbackPass(function(fs) {
          chrome.test.assertNe(undefined, fs.name);
          chrome.test.assertEq(
              fs.name.length - SYNCABLE_NAME_SUFFIX.length,
              fs.name.lastIndexOf(SYNCABLE_NAME_SUFFIX));
          chrome.test.assertNe(undefined, fs.root);
          chrome.test.assertTrue(fs.root.isFile || fs.root.isDirectory);
        }));
  },
]);
