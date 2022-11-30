// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function setupListener() {
  chrome.syncFileSystem.onServiceStatusChanged.addListener(checkEventReceived);
  chrome.syncFileSystem.requestFileSystem(function() {});
}

function checkEventReceived(serviceInfo) {
  chrome.test.assertEq("running", serviceInfo.state);
  chrome.test.assertEq("Test event description.", serviceInfo.description);
  chrome.test.succeed();
}

chrome.test.runTests([
  setupListener
]);
