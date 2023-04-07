// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryApiTest.Delete

const scriptUrl = '_test_resources/api_test/history/regular/common.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
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
    function deleteRangeTestStart() {
      removeItemRemovedListener();
      setItemVisitedListener(onAddedItem);
      setItemRemovedListener(deleteUrlTestVerification);
      populateHistory([GOOGLE_URL], function() { });
    }

    setItemRemovedListener(deleteRangeTestStart);
    chrome.history.deleteAll(() => {});
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

    function deleteRangeTestStart(removed) {
      removeItemRemovedListener();
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the first URL:
        chrome.history.deleteRange(
          {'startTime': eventTimes.before,
           'endTime': eventTimes.between},
          function() {});
      });
    }

    setItemRemovedListener(deleteRangeTestStart);
    chrome.history.deleteAll(() => {});
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

    function deleteRangeTestStart() {
      removeItemRemovedListener();
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering the second URL:
        chrome.history.deleteRange(
          {'startTime': eventTimes.between,
           'endTime': eventTimes.after},
          function() {});
      });
    }

    setItemRemovedListener(deleteRangeTestStart);
    chrome.history.deleteAll(() => {});
  },

  // Suppose we have time epochs x,y,z and history events A,B which occur
  // in the sequence x A y B z.  Delete range [x,z], check that both A
  // and B are removed.
  function deleteWholeRange() {
    var urls = [GOOGLE_URL, PICASA_URL];

    function deleteRangeTestVerification(removed) {
      removeItemRemovedListener();

      var query = { 'text': '' };
      chrome.history.search(query, function(results) {
        assertEq(0, results.length);

        // The test has succeeded.
        chrome.test.succeed();
      });
    }

    function deleteRangeTestStart(removed) {
      removeItemRemovedListener();
      setItemRemovedListener(deleteRangeTestVerification);
      addUrlsWithTimeline(urls, function(eventTimes) {
        // Remove the range covering both URLs:
        chrome.history.deleteRange(
          {'startTime': eventTimes.before,
           'endTime': eventTimes.after},
          function() {});
      });
    }

    setItemRemovedListener(deleteRangeTestStart);
    chrome.history.deleteAll(() => {});
  },

  // Delete a range with start time equal to end time.  See that nothing
  // is removed.
  function deleteEmptyRange() {
    var urls = [GOOGLE_URL, PICASA_URL];

    function deleteRangeTestVerification(removed) {
      removeItemRemovedListener();

      // Nothing should have been deleted.
      chrome.test.fail();
    }

    function deleteRangeTestStart(removed) {
      removeItemRemovedListener();
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
    }

    setItemRemovedListener(deleteRangeTestStart);
    chrome.history.deleteAll(() => {});
  }

])});
