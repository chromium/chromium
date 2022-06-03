// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryExtensionApiTest.MiscSearch

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  function basicSearch() {
    // basicSearch callback.
    function basicSearchTestVerification() {
      removeItemVisitedListener();
      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    // basicSearch entry point.
    chrome.history.deleteAll(function() {
      setItemVisitedListener(basicSearchTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  function lengthScopedSearch() {
    var urls = [GOOGLE_URL, PICASA_URL];
    var urlsAdded = 0;

    function lengthScopedSearchTestVerification() {
      // Ensure all urls have been added.
      urlsAdded += 1;
      if (urlsAdded < urls.length)
        return;

      removeItemVisitedListener();

      var query = { 'text': '', 'maxResults': 1 };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    };

    // lengthScopedSearch entry point.
    chrome.history.deleteAll(function() {
      setItemVisitedListener(lengthScopedSearchTestVerification);
      populateHistory(urls, function() { });
    });
  },
]);
