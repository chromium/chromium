// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function (launchData) {
  // Test that the isKioskSession field is |false| and the id and items fields
  // can be read in the launch data.
  chrome.test.runTests([
    function testFileHandler() {
      chrome.test.assertFalse(!launchData, "No launchData");
      chrome.test.assertFalse(launchData.isKioskSession,
          "launchData.isKioskSession incorrect");
      chrome.test.assertEq(launchData.id, "unknown",
          "launchData.id incorrect");
      chrome.test.assertEq(launchData.items.length, 1);
      chrome.test.assertEq(launchData.items[0].type,
          "application/octet-stream");
      chrome.test.assertTrue(
          chrome.fileSystem.retainEntry(launchData.items[0].entry) != null);

      chrome.test.succeed();
    }
  ]);
});
