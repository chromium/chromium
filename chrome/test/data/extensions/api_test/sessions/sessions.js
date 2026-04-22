// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const pages = [pageUrl('a'), pageUrl('b'), pageUrl('c')];
const firstWindowTabIds = [];
const recentlyClosedSecondWindowTabIds = [];
const recentlyClosedTabIds = [];
const recentlyClosedWindowIds = [];
const windowIds = [];

const callbackPass = chrome.test.callbackPass;
const callbackFail = chrome.test.callbackFail;
const assertEq = chrome.test.assertEq;
const assertFalse = chrome.test.assertFalse;
const assertTrue = chrome.test.assertTrue;

function pageUrl(letter) {
  return chrome.runtime.getURL(`${letter}.html`);
}

// Creates one window with tabs set to the urls in the array |tabUrls|.
// At least one url must be specified.
// The |callback| should look like function(windowId, tabIds) {...}.
function createWindow(tabUrls, callback) {
  chrome.windows.create({url: tabUrls}, function(win) {
    const newTabIds = [];
    win.tabs.forEach(function(tab) {
      newTabIds.push(tab.id);
    });
    callback(win.id, newTabIds);
  });
}

function callForEach(fn, calls, eachCallback, doneCallback) {
  if (!calls.length) {
    doneCallback();
    return;
  }
  fn.call(null, calls[0], callbackPass(function() {
            eachCallback.apply(null, arguments);
            callForEach(fn, calls.slice(1), eachCallback, doneCallback);
          }));
}

function checkEntries(expectedEntries, actualEntries) {
  assertEq(expectedEntries.length, actualEntries.length);
  expectedEntries.forEach(function(expected, i) {
    const actual = actualEntries[i];
    if (expected.tab) {
      assertTrue(actual.hasOwnProperty('tab'));
      assertFalse(actual.hasOwnProperty('window'));
      assertEq(expected.tab.url, actual.tab.url);
    } else {
      assertTrue(actual.hasOwnProperty('window'));
      assertFalse(actual.hasOwnProperty('tab'));
      assertEq(expected.window.tabsLength, actual.window.tabs.length);
    }
  });
}

function checkOnChangedEvent(expectedCallbackCount) {
  // The frequency in ms between checking whether the right events have
  // fired. Every 10 attempts progress is logged.
  const retryPeriod = 100;

  let callbackCount = 0;
  const done = chrome.test.listenForever(
      chrome.sessions.onChanged,
      function() {
        callbackCount++;
      },
  );

  return function() {
    let retry = 0;
    const checkEvent = function() {
      if (callbackCount < expectedCallbackCount) {
        retry++;
        if (retry % 10 == 0) {
          console.log(
              `Waiting for ${expectedCallbackCount - callbackCount} ` +
              'more onChanged events');
        }
        window.setTimeout(checkEvent, retryPeriod);
      } else {
        assertEq(callbackCount, expectedCallbackCount);
        done();
      }
    };
    window.setTimeout(checkEvent, retryPeriod);
  };
}

chrome.test.runTests([
  // After setupWindows
  //
  //  Window1: a,b,c
  //  Window2: a,b
  //  Window3: a,b
  //
  // After retrieveClosedTabs:
  //
  //  Window1: c
  //  Window2: a,b
  //  Window3: a,b
  //  ClosedList: a,b
  //
  // After retrieveClosedWindows:
  //
  //  Window1: c
  //  ClosedList: Window2,Window3,a,b
  function setupWindows() {
    const callArgs = [
      pages,
      pages.slice(0, 2),
      pages.slice(0, 2),
    ];
    callForEach(
        createWindow,
        callArgs,
        function each(winId, tabIds) {
          windowIds.push(winId);
        },
        function done() {
          chrome.tabs.query(
              {windowId: windowIds[0]}, callbackPass(function(tabs) {
                assertEq(pages.length, tabs.length);
                tabs.forEach(function(tab) {
                  firstWindowTabIds.push(tab.id);
                });
              }));
          chrome.windows.getAll(
              {populate: true},
              callbackPass(function(win) {
                assertEq(callArgs.length + 1, win.length);
              }),
          );
        },
    );
  },

  function retrieveClosedTabs() {
    // Check that the recently closed list contains what we expect
    // after removing tabs.
    const checkEvent = checkOnChangedEvent(2);

    callForEach(
        chrome.tabs.remove,
        firstWindowTabIds.slice(0, 2).reverse(),
        function each() {},
        function done() {
          chrome.sessions.getRecentlyClosed(
              {maxResults: 2},
              callbackPass(function(entries) {
                const expectedEntries = [
                  {tab: {url: pages[0]}},
                  {tab: {url: pages[1]}},
                ];
                checkEntries(expectedEntries, entries);
                entries.forEach(function(entry) {
                  recentlyClosedTabIds.push(entry.tab.sessionId);
                });
                checkEvent();
              }),
          );
        },
    );
  },

  function retrieveClosedWindows() {
    // Check that the recently closed list contains what we expect
    // after removing windows.
    const checkEvent = checkOnChangedEvent(2);

    callForEach(
        chrome.windows.remove,
        windowIds.slice(1, 3).reverse(),
        function each() {},
        function done() {
          chrome.sessions.getRecentlyClosed(
              {maxResults: 2},
              callbackPass(function(entries) {
                const expectedEntries = [
                  {window: {tabsLength: 2}},
                  {window: {tabsLength: 2}},
                ];
                checkEntries(expectedEntries, entries);
                entries[0].window.tabs.forEach(function(tab) {
                  recentlyClosedSecondWindowTabIds.push(tab.sessionId);
                });
                entries.forEach(function(entry) {
                  recentlyClosedWindowIds.push(entry.window.sessionId);
                });
                checkEvent();
              }),
          );
        },
    );
  },

  function retrieveClosedEntries() {
    // Check that the recently closed list contains what we expect
    // after removing tabs and windows.
    chrome.sessions.getRecentlyClosed(
        callbackPass(function(entries) {
          const expectedEntries = [
            {window: {tabsLength: 2}},
            {window: {tabsLength: 2}},
            {tab: {url: pages[0]}},
            {tab: {url: pages[1]}},
          ];
          checkEntries(expectedEntries, entries);
          assertEq(
              recentlyClosedTabIds.length + recentlyClosedWindowIds.length,
              entries.length);
        }),
    );
  },

  function retrieveMaxEntries() {
    // Check that the recently closed list contains what we expect
    // after removing tabs and windows.
    chrome.sessions.getRecentlyClosed(
        {maxResults: 25},
        callbackPass(function(entries) {
          const expectedEntries = [
            {window: {tabsLength: 2}},
            {window: {tabsLength: 2}},
            {tab: {url: pages[0]}},
            {tab: {url: pages[1]}},
          ];
          checkEntries(expectedEntries, entries);
          assertEq(
              recentlyClosedTabIds.length + recentlyClosedWindowIds.length,
              entries.length);
        }),
    );
  },

  function restoreClosedTabs() {
    const checkEvent = checkOnChangedEvent(2);

    chrome.windows.get(
        windowIds[0],
        {populate: true},
        callbackPass(function(win) {
          const tabCountBeforeRestore = win.tabs.length;
          chrome.sessions.restore(
              recentlyClosedTabIds[0], function(tab_session) {
                assertEq(pages[0], tab_session.tab.url);
              });
          chrome.sessions.restore(
              recentlyClosedTabIds[1], function(tab_session) {
                assertEq(pages[1], tab_session.tab.url);
              });
          chrome.windows.get(
              windowIds[0],
              {populate: true},
              callbackPass(function(win) {
                assertEq(tabCountBeforeRestore + 2, win.tabs.length);
                win.tabs.forEach(function(tab, i) {
                  assertEq(pages[i++], tab.url);
                });
                checkEvent();
              }),
          );
        }),
    );
  },

  function restoreTabInClosedWindow() {
    const checkEvent = checkOnChangedEvent(1);

    chrome.windows.getAll(
        {populate: true}, callbackPass(function(win) {
          const windowCountBeforeRestore = win.length;
          chrome.sessions.restore(
              recentlyClosedSecondWindowTabIds[0],
              callbackPass(function(tab_session) {
                assertEq(pages[0], tab_session.tab.url);
                chrome.windows.getAll(
                    {populate: true},
                    callbackPass(function(win) {
                      assertEq(windowCountBeforeRestore + 1, win.length);
                      assertEq(1, win[win.length - 1].tabs.length);
                      assertEq(pages[0], win[win.length - 1].tabs[0].url);
                      checkEvent();
                    }),
                );
              }),
          );
        }));
  },

  function restoreClosedWindows() {
    const checkEvent = checkOnChangedEvent(1);

    chrome.windows.getAll(
        {populate: true}, callbackPass(function(win) {
          const windowCountBeforeRestore = win.length;
          chrome.sessions.restore(
              recentlyClosedWindowIds[0], function(winSession) {
                assertEq(1, winSession.window.tabs.length);
                checkEvent();
              });
          function done() {
            chrome.windows.getAll(
                {populate: true},
                callbackPass(function(win) {
                  assertEq(windowCountBeforeRestore + 1, win.length);
                }),
            );
          }
        }));
  },

  function restoreSameEntryTwice() {
    chrome.windows.getAll(
        {populate: true}, callbackPass(function(win) {
          const windowCountBeforeRestore = win.length;
          const id = recentlyClosedWindowIds[0];
          chrome.sessions.restore(
              id,
              callbackFail(
                  `Invalid session id: "${id}".`,
                  function() {
                    chrome.windows.getAll(
                        {populate: true},
                        callbackPass(function(win) {
                          assertEq(windowCountBeforeRestore, win.length);
                        }),
                    );
                  }),
          );
        }));
  },

  function restoreInvalidEntries() {
    chrome.windows.getAll(
        {populate: true}, callbackPass(function(win) {
          const windowCountBeforeRestore = win.length;
          chrome.sessions.restore(
              '-1',
              callbackFail(
                  'Invalid session id: "-1".',
                  function() {
                    chrome.windows.getAll(
                        {populate: true},
                        callbackPass(function(win) {
                          assertEq(windowCountBeforeRestore, win.length);
                        }),
                    );
                  }),
          );
        }));
  },

  function restoreMostRecentEntry() {
    const checkEvent = checkOnChangedEvent(1);

    chrome.windows.getAll(
        {populate: true}, callbackPass(function(win) {
          const windowCountBeforeRestore = win.length;
          chrome.sessions.restore(callbackPass(function(winSession) {
            assertEq(2, winSession.window.tabs.length);
            chrome.windows.getAll(
                {populate: true},
                callbackPass(function(win) {
                  assertEq(windowCountBeforeRestore + 1, win.length);
                  checkEvent();
                }),
            );
          }));
        }));
  },

  function checkRecentlyClosedListEmpty() {
    chrome.windows.getAll(
        {populate: true}, callbackPass(function(win) {
          const windowCountBeforeRestore = win.length;
          chrome.sessions.restore(
              callbackFail(
                  'There are no recently closed sessions.',
                  function() {
                    chrome.windows.getAll(
                        {populate: true},
                        callbackPass(function(win) {
                          assertEq(windowCountBeforeRestore, win.length);
                        }),
                    );
                  }),
          );
        }));
  },
]);
