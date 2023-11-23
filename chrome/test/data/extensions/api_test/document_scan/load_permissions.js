// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testLoadPermissions() {
  chrome.test.runTests([
    function apiFunctionExists() {
      chrome.test.assertTrue(!!chrome.documentScan);
      chrome.test.assertTrue(!!chrome.documentScan.scan);
      chrome.test.succeed();
    },
  ]);
}

testLoadPermissions();
