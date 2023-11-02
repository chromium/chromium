// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var assertEq = chrome.test.assertEq;
var assertFalse = chrome.test.assertFalse;
var assertLastError = chrome.test.assertLastError;
var assertNoLastError = chrome.test.assertNoLastError;
var assertTrue = chrome.test.assertTrue;
var succeed = chrome.test.succeed;

const SEARCH_WORDS = 'search words';

chrome.test.runTests([

  // Error if search string is empty.
  function QueryEmpty() {
    chrome.search.query({text: ''}, function() {
      assertLastError('Empty text parameter.');
      succeed();
    });
  },

  // Display results in current tab if no disposition is provided.
  function QueryPopulatedDispositionEmpty() {
    chrome.tabs.create({}, (tab) => {
      waitForTabAndPass(tab.id);
      chrome.search.query({text: SEARCH_WORDS}, () => {});
    });
  },

  // Display results in current tab if said disposition is provided.
  function QueryPopulatedDispositionCurrentTab() {
    chrome.tabs.create({}, (tab) => {
      waitForTabAndPass(tab.id);
      chrome.search.query({text: SEARCH_WORDS, disposition: 'CURRENT_TAB'});
    });
  },

  // Display results in new tab if said disposition is provided.
  function QueryPopulatedDispositionNewTab() {
    chrome.tabs.query({}, (initialTabs) => {
      let initialTabIds = initialTabs.map(tab => tab.id);
      Promise
          .all([
            waitForAnyTab(),
            new Promise(resolve => {
              chrome.search.query(
                  {text: SEARCH_WORDS, disposition: 'NEW_TAB'}, () => {
                    chrome.tabs.query(
                        {active: true, currentWindow: true}, (tabs) => {
                          assertEq(1, tabs.length);
                          // A new tab should have been created.
                          assertFalse(initialTabIds.includes(tabs[0].id));
                          resolve();
                        });
                  });
            }),
          ])
          .then(() => {
            succeed();
          });
    });
  },

  // Display results in new window if said disposition is provided.
  function QueryPopulatedDispositionNewWindow() {
    chrome.windows.getAll({}, (initialWindows) => {
      let initialWindowIds = initialWindows.map(window => window.id);
      Promise
          .all([
            waitForAnyTab(),
            new Promise((resolve) => {
              chrome.search.query(
                  {text: SEARCH_WORDS, disposition: 'NEW_WINDOW'}, () => {
                    chrome.windows.getAll({}, (windows) => {
                      let window = windows.find(
                          window => !initialWindowIds.includes(window.id));
                      assertEq(windows.length, initialWindowIds.length + 1);
                      assertTrue(!!window);
                      resolve();
                    });
                  });
            }),
          ])
          .then(() => {
            succeed();
          });
    });
  },

  // Display results in specified tab if said tabId is provided.
  function QueryPopulatedTabIDValid() {
    chrome.tabs.create({}, (tab) => {
      waitForTabAndPass(tab.id);
      chrome.search.query({text: SEARCH_WORDS, tabId: tab.id});
    });
  },

  // Error if tab id invalid.
  function QueryPopulatedTabIDInvalid() {
    chrome.search.query({text: SEARCH_WORDS, tabId: -1}, () => {
      assertLastError('No tab with id: -1.');
      succeed();
    });
  },

  // Error if both tabId and Disposition populated.
  function QueryAndDispositionPopulatedTabIDValid() {
    chrome.tabs.query({active: true}, (tabs) => {
      chrome.search.query(
          {text: SEARCH_WORDS, tabId: tabs[0].id, disposition: 'NEW_TAB'},
          () => {
            assertLastError('Cannot set both \'disposition\' and \'tabId\'.');
            succeed();
          });
    });
  },
]);

function waitForTab(tabIdExpected) {
  return new Promise((resolve) => {
    chrome.tabs.onUpdated.addListener(function listener(
        tabId, changeInfo, tab) {
      if ((tabIdExpected != -1 && tabId != tabIdExpected) ||
          changeInfo.status !== 'complete') {
        return;  // Not our tab.
      }
      // Note: make sure to stop listening to future events, so that this
      // doesn't affect future tests.
      chrome.tabs.onUpdated.removeListener(listener);
      // The tab finished loading. It should be on google (the default
      // search engine).
      assertEq('www.google.com', new URL(tab.url).hostname);
      resolve();
    });
  });
};

function waitForAnyTab() {
  return waitForTab(-1);
};

function waitForTabAndPass(tabId) {
  waitForTab(tabId).then(succeed);
}