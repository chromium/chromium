// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function fileAccessAllowed() {
      var req = new XMLHttpRequest();

      var url = config.testDataDirectory + "/../test_file.txt";
      chrome.test.log("Requesting url: " + url);
      req.open("GET", url, true);

      req.onload = function() {
        chrome.test.assertEq("Hello!", req.responseText);
        chrome.test.succeed();
      }
      req.onerror = function() {
        chrome.test.log("status: " + req.status);
        chrome.test.log("text: " + req.responseText);
        chrome.test.fail("Unexpected error for url: " + url);
      }

      req.send(null);
    }
  ]);
});
