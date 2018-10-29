// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryApiTest.Delete

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  // All the tests require a blank state to start from.  If this fails,
  // expect all other history tests (HistoryExtensionApiTest.*) to fail.
  function clearHistory() {
    chrome.history.deleteAll(pass(function() {
      countItemsInHistory(pass(function(count) {
        assertEq(0, count);
      }));
    }));
  },

  function deleteUrl() {
    function deleteUrlTestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(0, results.length);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

    function onAddedItem() {
      removeItemVisitedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        chrome.history.deleteUrl({ 'url': GOOGLE_URL });
      });
    }

    // deleteUrl entry point.
    chrome.history.deleteAll(function() {
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteUrlTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    });
  },

  // Suppose we have time epochs x,y,z and history events A,B which occur
  // in the sequence x A y B z.  Delete range [x,y], check that only A is
  // removed.
  function deleteStartRange() {
    var urls = [GOOGLE_URL, PICASA_URL];

    function deleteRangeTestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(PICASA_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

    chrome.history.deleteAll(function() {
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the first URL:
        chrome.history.deleteRange(
          {'startTime': eventTimes.before,
           'endTime': eventTimes.between},
          function() {});
      });
    });
  },

  // Suppose we have time epochs x,y,z and history events A,B which occur
  // in the sequence x A y B z.  Delete range [y,z], check that only B is
  // removed.
  function deleteEndRange() {
    var urls = [GOOGLE_URL, PICASA_URL];

    function deleteRangeTestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(1, results.length);
        assertEq(GOOGLE_URL, results[0].url);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

    chrome.history.deleteAll(function() {
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the second URL:
        chrome.history.deleteRange(
          {'startTime': eventTimes.between,
           'endTime': eventTimes.after},
          function() {});
      });
    });
  },

  // Suppose we have time epochs x,y,z and history events A,B which occur
  // in the sequence x A y B z.  Delete range [x,z], check that both A
  // and B are removed.
  function deleteWholeRange() {
    var urls = [GOOGLE_URL, PICASA_URL];

    function deleteRangeTestVerification() {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(0, results.length);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

    chrome.history.deleteAll(function() {
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering both URLs:
        chrome.history.deleteRange(
          {'startTime': eventTimes.before,
           'endTime': eventTimes.after},
          function() {});
      });
    });
  },

  // Delete a range with start time equal to end time.  See that nothing
  // is removed.
  function deleteEmptyRange() {
    var urls = [GOOGLE_URL, PICASA_URL];

    function deleteRangeTestVerification() {
      removeItemRemovedListener();

      // Nothing should have been deleted.
      chrome.test.fail();
    }

    chrome.history.deleteAll(function() {
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove an empty range.
        chrome.history.deleteRange(
          {'startTime': eventTimes.between,
           'endTime': eventTimes.between},
          function() {
            var query = { 'text': '' };
            chrome.history.search(query, function(results) {
              // Nothing should have been deleted.
              assertEq(2, results.length);
              chrome.test.succeed();
            });
          });
      });
    });
  }
]);
