// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onFileStatusChanged.addListener(fileInfoReceived);
  chrome.syncFileSystem.requestFileSystem(function() {});
}

function fileInfoReceived(fileInfo) {
  // FileEntry object fields.
  var fileEntry = fileInfo.fileEntry;
  chrome.test.assertEq("foo.txt", fileEntry.name);
  chrome.test.assertEq("/foo.txt", fileEntry.fullPath);
  chrome.test.assertTrue(fileEntry.isFile);
  chrome.test.assertFalse(fileEntry.isDirectory);

  chrome.test.assertEq("synced", fileInfo.status);
  chrome.test.assertEq("added", fileInfo.action);
  chrome.test.assertEq("remote_to_local", fileInfo.direction);
  chrome.test.succeed();
}

chrome.test.runTests([
  setupListener
]);
