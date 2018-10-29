// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryExtensionApiTest.SearchAfterAdd

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  function searchAfterAdd() {
    chrome.history.deleteAll(function() {
      var VALID_URL = 'http://www.google.com/';
      chrome.history.addUrl({url: VALID_URL}, function() {
        chrome.history.search({text: ''}, function(historyItems) {
          assertEq(1, historyItems.length);
          assertEq(VALID_URL, historyItems[0].url);
          chrome.test.succeed();
        });
      });
    });
  }
]);
