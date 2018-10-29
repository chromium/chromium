// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryExtensionApiTest.TimedSearch

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  // Give time epochs x,y,z and history events A,B which occur in the sequence
  // x A y B z, test that searching in [x,y] finds only A.
  function timeScopedSearchStartRange() {
    var urls = [GOOGLE_URL, PICASA_URL];
    chrome.history.deleteAll(function() {
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the first URL:
        chrome.history.search(
          {'text': '',
           'startTime': eventTimes.before,
           'endTime': eventTimes.between},
          function(historyItems) {
            assertEq(1, historyItems.length);
            assertEq(GOOGLE_URL, historyItems[0].url);
            chrome.test.succeed();
          });
      });
    });
  },

  // Give time epochs x,y,z and history events A,B which occur in the sequence
  // x A y B z, test that searching in [y,z] finds only B.
  function timeScopedSearchEndRange() {
    var urls = [GOOGLE_URL, PICASA_URL];
    chrome.history.deleteAll(function() {
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the first URL:
        chrome.history.search(
          {'text': '',
           'startTime': eventTimes.between,
           'endTime': eventTimes.end},
          function(historyItems) {
            assertEq(1, historyItems.length);
            assertEq(PICASA_URL, historyItems[0].url);
            chrome.test.succeed();
          });
      });
    });
  },

  // Give time epochs x,y,z and history events A,B which occur in the sequence
  // x A y B z, test that searching in [y,y] finds nothing.
  function timeScopedSearchEmptyRange() {
    var urls = [GOOGLE_URL, PICASA_URL];
    chrome.history.deleteAll(function() {
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the first URL:
        chrome.history.search(
          {'text': '',
           'startTime': eventTimes.between,
           'endTime': eventTimes.between},
          function(historyItems) {
            assertEq(0, historyItems.length);
            chrome.test.succeed();
          });
      });
    });
  },

  function searchWithIntegerTimes() {
    chrome.history.deleteAll(function() {
      // Search with an integer time range.
      var query = { 'text': '',
                    'startTime': 0,
                    'endTime': 123456789 };
      chrome.history.search(query, function(results) {
        assertEq(0, results.length);
        chrome.test.succeed();
      });
    });
  }
]);
