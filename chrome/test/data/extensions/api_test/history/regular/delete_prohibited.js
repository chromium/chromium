// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryApiTest.DeleteProhibited

var PROHIBITED_ERR = "Browsing history is not allowed to be deleted.";

// The maximum number of tries querying the history database to make sure that
// the initial addUrl calls succeeded.
var MAX_HISTORY_TRIES = 10;

function deleteProhibitedTestVerification() {
  removeItemRemovedListener();
  chrome.test.fail("Delete was prohibited, but succeeded anyway.");
}

// Orders history results by ID for reliable array comparison.
function sortResults(a, b) {
  return (a.id - b.id);
}

// Returns true if both expected URLs are in the result list.
function verifyHistory(resultList) {
  var hasGoogle = false;
  var hasPicasa = false;
  for (var i = 0; i < resultList.length; i++) {
    if (resultList[i].url == GOOGLE_URL)
      hasGoogle = true;
    else if (resultList[i].url == PICASA_URL)
      hasPicasa = true;
  }
  return (hasGoogle && hasPicasa);
}

// Runs the provided function (which must be curried with arguments if needed)
// and verifies that it both returns an error and does not delete any items from
// history.
function verifyNoDeletion(testFunction) {
  setItemRemovedListener(deleteProhibitedTestVerification);

  // Add two URLs, wait if necessary to make sure they both show up in the
  // history results, then run the provided test function. Re-query the history
  // to make sure the test function had no effect.
  var query = { 'text': '' };
  chrome.history.addUrl({ url: GOOGLE_URL }, pass(function() {
    chrome.history.addUrl({ url: PICASA_URL }, pass(function() {
      // Humans use 1-based counting.
      var tries = 1;
      chrome.history.search(query, pass(function lambda(resultsBefore) {
        if (verifyHistory(resultsBefore)) {
          // Success: proceed with the test.
          if (tries > 1) {
            console.log("Warning: Added URLs took " + tries + " tries to " +
                        "show up in history. See http://crbug.com/176828.");
          }
          testFunction(fail(PROHIBITED_ERR, function() {
            chrome.history.search(query, pass(function(resultsAfter) {
              assertEq(resultsBefore.sort(sortResults),
                      resultsAfter.sort(sortResults));
              removeItemRemovedListener();
            }));
          }));
        } else if (tries < MAX_HISTORY_TRIES) {
          // Results not yet in history: try again. See http://crbug.com/176828.
          ++tries;
          waitAFewSeconds(0.1, pass(function() {
            chrome.history.search(query, pass(lambda));
          }));
        } else {
          // Too many tries: fail.
          chrome.test.fail("Added URLs never showed up in the history. " +
                           "See http://crbug.com/176828.");
        }
      }));
    }));
  }));
}

// runHistoryTestFns is defined in ./common.js .
runHistoryTestFns([
  function deleteUrl() {
    verifyNoDeletion(function(callback) {
      chrome.history.deleteUrl({ 'url': GOOGLE_URL }, callback);
    });
  },

  function deleteRange() {
    var now = new Date();
    verifyNoDeletion(function(callback) {
      chrome.history.deleteRange(
          { 'startTime': 0, 'endTime': now.getTime() + 1000.0 }, callback);
    });
  },

  function deleteAll() {
    verifyNoDeletion(chrome.history.deleteAll);
  }
]);
