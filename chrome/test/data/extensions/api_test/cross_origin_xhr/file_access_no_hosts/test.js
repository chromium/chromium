// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function fileAccessNotAllowed() {
      var req = new XMLHttpRequest();

      var url = config.testDataDirectory + '/../test_file.txt';
      chrome.test.log('Requesting url: ' + url);
      req.open('GET', url, true);

      req.onload = function() {
        chrome.test.fail('Unexpected success for url: ' + url);
      }
      req.onerror = function() {
        chrome.test.assertEq(0, req.status);
        chrome.test.succeed();
      }

      req.send(null);
    }
  ]);
});
