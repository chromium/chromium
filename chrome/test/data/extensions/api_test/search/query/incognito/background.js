// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertNoLastError = chrome.test.assertNoLastError;
var assertTrue = chrome.test.assertTrue;
var succeed = chrome.test.succeed;

const SEARCH_WORDS = 'search words';

chrome.test.runTests([

  // Verify search results shown in specified incognito tab.
  function IncognitoSpecificTab() {
    chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
      const tab = tabs[0];
      testHelper(tabs, {text: SEARCH_WORDS, tabId: tab.id});
    });
  },

  // Verify search results shown in current incognito tab.
  function IncognitoNoDisposition() {
    chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
      testHelper(tabs, {text: SEARCH_WORDS});
    });
  },
]);

var testHelper = (tabs, queryInfo) => {
  assertEq(1, tabs.length);
  const tab = tabs[0];
  // The browser test should have spun up an incognito browser, which
  // should be active.
  assertTrue(tab.incognito);
  addTabListener(tab.id);
  chrome.search.query(queryInfo, () => {
    assertNoLastError();
    chrome.tabs.query({windowId: tab.windowId}, (tabs) => {});
  });
};

let addTabListener = (tabIdExpected) => {
  chrome.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
    if (tabId != tabIdExpected || changeInfo.status !== 'complete') {
      return;  // Not our tab.
    }
    // Note: make sure to stop listening to future events, so that this
    // doesn't affect future tests.
    chrome.tabs.onUpdated.removeListener(listener);
    // The tab finished loading. It should be on google (the default
    // search engine).
    const hostname = new URL(tab.url).hostname;
    assertEq('www.google.com', hostname);
    succeed();
  });
};
