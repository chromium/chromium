// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Calls to chrome.experimental.* functions should fail, since this extension
// has not declared that permission.

chrome.test.runTests([
  function experimental() {
    chrome.tabs.query({active: true}, function(tabs) {
      try {
        // If/when chrome.experimental.history is moved out of
        // experimental, this test needs to be updated.
        chrome.experimental.history.getMostVisited(
          {}, function(results) {
          chrome.test.fail();
        });
      } catch (e) {
        chrome.test.succeed();
      }
    });
  }
]);
