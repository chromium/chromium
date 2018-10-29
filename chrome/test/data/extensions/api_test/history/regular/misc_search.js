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

  function fullTextSearch() {
    chrome.history.deleteAll(function() {
      // The continuation of the test after the windows have been opened.
      var validateTest = function() {
        // Continue with the test.
        // A title search for www.a.com should find a.
        var query = { 'text': 'www.a.com' };
        chrome.history.search(query, function(results) {
          assertEq(1, results.length);
          assertEq(A_RELATIVE_URL, results[0].url);

          // Text in the body of b.html.
          query = { 'text': 'ABCDEFGHIJKLMNOPQRSTUVWXYZ' };
          chrome.history.search(query, function(results) {
            assertEq(1, results.length);
            assertEq(B_RELATIVE_URL, results[0].url);

            // The test has succeeded.
            chrome.test.succeed();
          });
        });
      };

      // Setup a callback object for tab events.
      var urls = [A_RELATIVE_URL, B_RELATIVE_URL];
      var tabIds = [];

      function listenerCallback() {
        if (tabIds.length < urls.length) {
          return;
        }

        // Ensure both tabs have completed loading.
        for (var index = 0, id; id = tabIds[index]; index++) {
          if (!tabsCompleteData[id] ||
          tabsCompleteData[id] != 'complete') {
            return;
          };
        }

        // Unhook callbacks.
        tabCompleteCallback = null;
        chrome.tabs.onUpdated.removeListener(tabsCompleteListener);

        // Allow indexing to occur.
        waitAFewSeconds(3, function() {
          validateTest();
        });
      }

      tabCompleteCallback = listenerCallback;
      chrome.tabs.onUpdated.addListener(tabsCompleteListener);

      // Navigate to a few pages.
      urls.forEach(function(url) {
        chrome.tabs.create({ 'url': url }, function(tab) {
          tabIds.push(tab.id);
        });
      });
    });
  }
]);
