// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

var GOOGLE_URL = 'http://www.google.com/';
var PICASA_URL = 'http://www.picasa.com/';

/**
 * Object used for listening to the chrome.history.onVisited events.  The
 * global object 'itemVisitedCallback' stores the last item received.
 */
var itemVisitedCallback = null;
function itemVisitedListener(visited) {
  if (null != itemVisitedCallback) {
    itemVisitedCallback(visited);
  }
}

function removeItemVisitedListener() {
  chrome.history.onVisited.removeListener(itemVisitedListener);
  itemVisitedCallback = null;
}

function setItemVisitedListener(callback) {
  chrome.history.onVisited.addListener(itemVisitedListener);
  itemVisitedCallback = callback;
}

/**
 * An object used for listening to the chrome.history.onVisitRemoved events.
 * The global object 'itemRemovedInfo' stores the information from the last
 * callback.
 */
var itemRemovedCallback = null;
function itemRemovedListener(removed) {
  if (null != itemRemovedCallback) {
    itemRemovedCallback(removed);
  };
};

function removeItemRemovedListener() {
  chrome.history.onVisited.removeListener(itemRemovedListener);
  itemRemovedCallback = null;
}

function setItemRemovedListener(callback) {
  chrome.history.onVisitRemoved.addListener(itemRemovedListener);
  itemRemovedCallback = callback;
}

/**
 * An object used for listening to the chrome.history.onVisitRemoved events.
 * Set 'tabCompleteCallback' to a function to add extra processing to the
 * callback.  The global object 'tabsCompleteData' contains a list of the
 * last known state of every tab.
 */
var tabCompleteCallback = null;
var tabsCompleteData = {};
function tabsCompleteListener(tabId, changeInfo) {
  if (changeInfo && changeInfo.status) {
    tabsCompleteData[tabId] = changeInfo.status;
  };
  if (null != tabCompleteCallback) {
    tabCompleteCallback();
  };
};

/**
 * Queries the entire history for items, calling the closure with an argument
 * specifying the the number of items in the query.
 * @param {function(number)} callback The closure.
 */
function countItemsInHistory(callback) {
  var query = {'text': ''};
  chrome.history.search(query, function(results) {
    callback(results.length);
  });
}

/**
 * Populates the history by calling addUrl for each url in the array urls.
 * @param {Array<string>} urls The array of urls to populate the history.
 * @param {function} callback Closure.
 */
function populateHistory(urls, callback) {
  var num_urls_added = 0;
  urls.forEach(function(url) {
    chrome.history.addUrl({ 'url': url }, function() {
      if (++num_urls_added == urls.length)
        callback()
    });
  });
}

/**
 * Tests call this function to invoke specific tests.
 * @param {Array<function>} testFns The tests to run.
 */
function runHistoryTestFns(testFns) {
  chrome.test.runTests(testFns);
}

/**
 * Add two URLs to the history.  Compute three times, in ms since the epoch:
 *    'before': A time before both URLs were added.
 *    'between': A time between the times the URLs were added.
 *    'after': A time after both were added.
 * All times are passed to |callback| as properties of its object parameter.
 * @param {Array<string>} urls An array of two URLs to add to the history.
 * @param {function(object)} callback Called with the times described above.
 */
function addUrlsWithTimeline(urls, callback) {
  // If a test needs more than two urls, this could be generalized.
  assertEq(2, urls.length);

  // Add the first URL now.
  chrome.history.addUrl({url: urls[0]}, function() {
    chrome.history.addUrl({url: urls[1]}, function() {
      // Use search to get the times of the two URLs, and compute times
      // to pass to the callback.
      chrome.history.search({text: ''}, function(historyItems) {
        // Check that both URLs were added.
        assertEq(urls.length, historyItems.length);

        // Don't assume anything about the order of history records in
        // |historyItems|.
        var firstUrlTime = Math.min(historyItems[0].lastVisitTime,
                                    historyItems[1].lastVisitTime);
        var secondUrlTime = Math.max(historyItems[0].lastVisitTime,
                                     historyItems[1].lastVisitTime);

        callback({
          before: firstUrlTime - 100.0,
          between: (firstUrlTime + secondUrlTime) / 2.0,
          after: secondUrlTime + 100.0
        });
      });
    });
  });
}
