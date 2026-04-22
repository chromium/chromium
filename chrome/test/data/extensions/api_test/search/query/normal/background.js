// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const assertEq = chrome.test.assertEq;
const assertFalse = chrome.test.assertFalse;
const assertLastError = chrome.test.assertLastError;
const assertNoLastError = chrome.test.assertNoLastError;
const assertTrue = chrome.test.assertTrue;
const succeed = chrome.test.succeed;

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
    chrome.tabs.create({}, async (tab) => {
      await waitForNewTab(tab.id);
      const navigated = waitForNavigationToGoogle(tab.id);
      chrome.search.query({text: SEARCH_WORDS}, () => {});
      await navigated;
      chrome.test.succeed();
    });
  },

  // Display results in current tab if said disposition is provided.
  function QueryPopulatedDispositionCurrentTab() {
    chrome.tabs.create({}, async (tab) => {
      await waitForNewTab(tab.id);
      const navigated = waitForNavigationToGoogle(tab.id);
      chrome.search.query({text: SEARCH_WORDS, disposition: 'CURRENT_TAB'});
      await navigated;
      chrome.test.succeed();
    });
  },

  // Display results in new tab if said disposition is provided.
  function QueryPopulatedDispositionNewTab() {
    chrome.tabs.query({}, (initialTabs) => {
      const initialTabIds = initialTabs.map(tab => tab.id);
      Promise
          .all([
            waitForGoogleInAnyTab(),
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
  async function QueryPopulatedDispositionNewWindow() {
    // TODO(crbug.com/394345948): Flaky on android-desktop-16-x64-rel-emu-tests
    // due to inconsistent URLs in new windows (newtab vs. google.com).
    const isAndroid = (await chrome.runtime.getPlatformInfo()).os === 'android';
    if (isAndroid) {
      chrome.test.succeed('skipped');
      return;
    }
    chrome.windows.getAll({}, (initialWindows) => {
      const initialWindowIds = initialWindows.map(window => window.id);
      Promise
          .all([
            waitForGoogleInAnyTab(),
            new Promise((resolve) => {
              chrome.search.query(
                  {text: SEARCH_WORDS, disposition: 'NEW_WINDOW'}, () => {
                    chrome.windows.getAll({}, (windows) => {
                      const window = windows.find(
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
    chrome.tabs.create({}, async (tab) => {
      await waitForNewTab(tab.id);
      const navigated = waitForNavigationToGoogle(tab.id);
      chrome.search.query({text: SEARCH_WORDS, tabId: tab.id});
      await navigated;
      chrome.test.succeed();
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

function waitForTab(tabIdExpected, expectedHostname) {
  return new Promise((resolve) => {
    chrome.tabs.onUpdated.addListener(
        function listener(tabId, changeInfo, tab) {
          if ((tabIdExpected != -1 && tabId != tabIdExpected) ||
              changeInfo.status !== 'complete') {
            return;  // Not our tab or not fully loaded.
          }
          // Stop listening to future events to avoid affecting future tests.
          chrome.tabs.onUpdated.removeListener(listener);

          // The tab finished loading. It should have expected hostname.
          assertEq(expectedHostname, new URL(tab.url).hostname);
          // Resolve the promise as the tab has navigated to the expected
          // hostname.
          resolve();
        });
  });
}

function waitForNewTab(tabId) {
  return waitForTab(tabId, 'newtab');
}

function waitForNavigationToGoogle(tabId) {
  return waitForTab(tabId, 'www.google.com');
}

function waitForGoogleInAnyTab() {
  return waitForTab(-1, 'www.google.com');
}
