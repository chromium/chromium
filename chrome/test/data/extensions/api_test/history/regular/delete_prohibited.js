// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// History api test for Chrome.
// browser_tests.exe --gtest_filter=HistoryApiTest.DeleteProhibited

var PROHIBITED_ERR = "Browsing history is not allowed to be deleted.";

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
      chrome.history.search(query, pass(function lambda(resultsBefore) {
        if (verifyHistory(resultsBefore)) {
          // Success: proceed with the test.
          testFunction(fail(PROHIBITED_ERR, function() {
            chrome.history.search(query, pass(function(resultsAfter) {
              assertEq(resultsBefore.sort(sortResults),
                      resultsAfter.sort(sortResults));
              removeItemRemovedListener();
            }));
          }));
        } else {
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
