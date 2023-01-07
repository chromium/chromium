// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var normalWindow, normalTab;
var incognitoWindow, incognitoTab;

var pass = chrome.test.callbackPass;
var fail = chrome.test.callbackFail;
var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;

chrome.test.getConfig(function(config) {
  chrome.test.runTests([
    function setupWindows() {
      // The test harness should have set us up with 2 windows: 1 incognito
      // and 1 regular. Verify that we can see both when we ask for it.
      chrome.windows.getAll({populate: true}, pass(function(windows) {
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
      }));
    },

    // Tests that we can update an incognito tab and get the event for it.
    function tabUpdate() {
      var newUrl = "about:blank";

      // Prepare the event listeners first.
      var done = chrome.test.listenForever(chrome.tabs.onUpdated,
        function(id, info, tab) {
          if (id == incognitoTab.id) {
            assertTrue(tab.incognito);
            assertEq(newUrl, tab.url);
            if (info.status == "complete")
              done();
          }
        });

      // Update our tabs.
      chrome.tabs.update(incognitoTab.id, {"url": newUrl}, pass());
    },

    // Tests a sequence of tab API calls.
    function tabNested() {
      // Setup our listeners. We check that the events fire in order.
      var eventCounter = 0;
      chrome.test.listenOnce(chrome.tabs.onCreated, function(tab) {
        assertEq(1, ++eventCounter);
        assertEq(incognitoTab.windowId, tab.windowId);
        assertTrue(tab.incognito);
      });
      chrome.test.listenOnce(chrome.tabs.onMoved, function(tabId) {
        assertEq(2, ++eventCounter);
      });
      chrome.test.listenOnce(chrome.tabs.onRemoved, function(tabId) {
        assertEq(3, ++eventCounter);
      });

      // Create, select, move, and close a tab in our incognito window.
      chrome.tabs.create({windowId: incognitoTab.windowId},
        pass(function(tab) {
          chrome.tabs.move(tab.id, {index: 0},
            pass(function(tab) {
              assertEq(incognitoTab.incognito, tab.incognito);
              chrome.tabs.remove(tab.id, pass());
            }));
        }));
    },

    // Tests content script injection to verify that the script can tell its
    // in incongnito.
    function contentScriptTestIncognito() {
      assertTrue(!chrome.extension.inIncognitoContext);

      var testUrl = "http://localhost:PORT/extensions/test_file.html"
          .replace(/PORT/, config.testServer.port);

      // Test that chrome.extension.inIncognitoContext is true for incognito
      // tabs.
      chrome.tabs.create({windowId: incognitoWindow.id, url: testUrl},
        pass(function(tab) {
          chrome.tabs.executeScript(tab.id,
            {code: 'document.title = chrome.extension.inIncognitoContext'},
            pass(function() {
              assertEq(undefined, chrome.runtime.lastError);
              chrome.tabs.get(tab.id, pass(function(tab) {
                  assertEq("true", tab.title);
                }));
            }));
        }));

      // ... and false for normal tabs.
      chrome.tabs.create({windowId: normalWindow.id, url: testUrl},
        pass(function(tab) {
          chrome.tabs.executeScript(tab.id,
            {code: 'document.title = chrome.extension.inIncognitoContext'},
            pass(function() {
              assertEq(undefined, chrome.runtime.lastError);
              chrome.tabs.get(tab.id, pass(function(tab) {
                  assertEq("false", tab.title);
                }));
            }));
        }));
    },

    // Tests that extensions can't move tabs between incognito and
    // non-incognito windows.
    function moveTabBetweenProfiles() {
      var errorMsg = "Tabs can only be moved between " +
                       "windows in the same profile.";

      // Create a tab in the non-incognito window...
      chrome.tabs.create({windowId: normalWindow.id, url: 'about:blank'},
        pass(function(tab) {
          // ... and then try to move it to the incognito window.
          chrome.tabs.move(tab.id,
            {windowId: incognitoWindow.id, index: 0}, fail(errorMsg));
        }));
    },

    // Tests that it is not possible to create a new window with a tab from a
    // different profile.
    function createWindowWithTabFromOtherProfile() {
      chrome.windows.create({
        tabId: incognitoTab.id,
      }, fail('Tabs can only be moved between windows in the same profile.'));
    },

    // Similarly, try to move a non-incognito tab to an incognito window.
    function createWindowWithTabFromOtherProfile2() {
      chrome.windows.create({
        tabId: normalTab.id,
        incognito: true,
      }, fail('Tabs can only be moved between windows in the same profile.'));
    }
  ]);
});
