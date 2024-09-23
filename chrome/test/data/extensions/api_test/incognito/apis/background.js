// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let normalWindow, normalTab;
let incognitoWindow, incognitoTab;

let assertEq = chrome.test.assertEq;
let assertTrue = chrome.test.assertTrue;

const crossProfileMoveErrorMsg =
    'Error: Tabs can only be moved between windows in the same profile.';

chrome.test.getConfig(config => {
  chrome.test.runTests([
    async function setupWindows() {
      // The test harness should have set us up with 2 windows: 1 incognito
      // and 1 regular. Verify that we can see both when we ask for it.
      let windows = await chrome.windows.getAll({populate: true});
      assertEq(2, windows.length);

      if (windows[0].incognito) {
        incognitoWindow = windows[0];
        normalWindow = windows[1];
      } else {
        normalWindow = windows[0];
        incognitoWindow = windows[1];
      }
      normalTab = normalWindow.tabs[0];
      incognitoTab = incognitoWindow.tabs[0];
      assertTrue(!normalWindow.incognito);
      assertTrue(incognitoWindow.incognito);
      chrome.test.succeed();
    },

    // Tests that we can update an incognito tab and get the event for it.
    async function tabUpdate() {
      let newUrl = 'about:blank';

      // Prepare the event listeners first.
      let done =
          chrome.test.listenForever(chrome.tabs.onUpdated, (id, info, tab) => {
            if (id == incognitoTab.id) {
              assertTrue(tab.incognito);
              assertEq(newUrl, tab.url);
              if (info.status == 'complete') {
                done();
              }
            }
          });

      // Update our tabs.
      await chrome.tabs.update(incognitoTab.id, {'url': newUrl});
    },

    // Tests a sequence of tab API calls.
    async function tabNested() {
      // Setup our listeners. We check that the events fire in order.
      let eventCounter = 0;
      chrome.test.listenOnce(chrome.tabs.onCreated, tab => {
        assertEq(1, ++eventCounter);
        assertEq(incognitoTab.windowId, tab.windowId);
        assertTrue(tab.incognito);
      });
      chrome.test.listenOnce(chrome.tabs.onMoved, (tabId, moveInfo) => {
        assertEq(2, ++eventCounter);
      });
      chrome.test.listenOnce(chrome.tabs.onRemoved, (tabId, removeInfo) => {
        assertEq(3, ++eventCounter);
      });

      // Create, select, move, and close a tab in our incognito window.
      let newTab = await chrome.tabs.create({windowId: incognitoTab.windowId});
      let movedTab = await chrome.tabs.move(newTab.id, {index: 0});
      assertEq(incognitoTab.incognito, movedTab.incognito);
      await chrome.tabs.remove(movedTab.id);
    },

    // Tests content script injection to verify that the script can tell its
    // in incongnito.
    async function contentScriptTestIncognito() {
      assertTrue(!chrome.extension.inIncognitoContext);

      let testUrl = 'http://localhost:PORT/extensions/test_file.html'.replace(
          /PORT/, config.testServer.port);

      // Test that chrome.extension.inIncognitoContext is true for incognito
      // tabs.
      let incognitoTab = await chrome.tabs.create(
          {windowId: incognitoWindow.id, url: testUrl});

      await chrome.scripting.executeScript({
        target: {
          tabId: incognitoTab.id,
        },
        func: () => {
          document.title = chrome.extension.inIncognitoContext;
        }
      });

      // Get the title of the tab after the script has run.
      incognitoTab = await chrome.tabs.get(incognitoTab.id);
      assertEq('true', incognitoTab.title);

      // ... and false for normal tabs.
      let nonIncognitoTab =
          await chrome.tabs.create({windowId: normalWindow.id, url: testUrl});

      await chrome.scripting.executeScript({
        target: {
          tabId: nonIncognitoTab.id,
        },
        func: () => {
          document.title = chrome.extension.inIncognitoContext;
        }
      });

      // Get the title of the tab after the script has run.
      nonIncognitoTab = await chrome.tabs.get(nonIncognitoTab.id);
      assertEq('false', nonIncognitoTab.title);
      chrome.test.succeed();
    },

    // Tests that extensions can't move tabs between incognito and
    // non-incognito windows.
    async function moveTabBetweenProfiles() {
      // Create a tab in the non-incognito window...
      let tab = await chrome.tabs.create(
          {windowId: normalWindow.id, url: 'about:blank'});
      // ... and then try to move it to the incognito window.
      await chrome.test.assertPromiseRejects(
          chrome.tabs.move(tab.id, {windowId: incognitoWindow.id, index: 0}),
          crossProfileMoveErrorMsg);
      chrome.test.succeed();
    },

    // Tests that it is not possible to create a new window with a tab from a
    // different profile.
    async function createWindowWithTabFromOtherProfile() {
      await chrome.test.assertPromiseRejects(
          chrome.windows.create({
            tabId: incognitoTab.id,
          }),
          crossProfileMoveErrorMsg);
      chrome.test.succeed();
    },

    // Similarly, try to move a non-incognito tab to an incognito window.
    async function createWindowWithTabFromOtherProfile2() {
      await chrome.test.assertPromiseRejects(
          chrome.windows.create({tabId: normalTab.id, incognito: true}),
          crossProfileMoveErrorMsg);
      chrome.test.succeed();
    }
  ]);
});
