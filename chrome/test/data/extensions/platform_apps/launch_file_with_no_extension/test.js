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
      chrome.test.assertEq(launchData.id, "text",
          "launchData.id incorrect");
      chrome.test.assertEq(launchData.items.length, 1);
      chrome.test.assertTrue(
          chrome.fileSystem.retainEntry(launchData.items[0].entry) != null);

      launchData.items[0].entry.file(function(file) {
        var reader = new FileReader();
        reader.onloadend = function(e) {
          chrome.test.assertEq(
              reader.result.indexOf("This is a test. Word."), 0);
          chrome.test.succeed();
        };
        reader.onerror = function(e) {
          chrome.test.fail("Error reading file contents.");
        };
        reader.readAsText(file);
      });
    }
  ]);
});
