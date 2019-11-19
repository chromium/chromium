// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabProps = [];
const NEW_TAB_URL = 'chrome://newtab/';

chrome.test.runTests([
  // Get the info for any tabs already exist.
  function testTabQueryInitial() {
    try {
      chrome.tabs.query({currentWindow: true}, function(tabs) {
        chrome.test.assertEq(1, tabs.length);
        tabProps.push({id: tabs[0].id, url: tabs[0].url});
        chrome.test.succeed();
      });
    } catch(e) {
      chrome.test.fail(e);
    }
  },
  // Create a new tab. Use an onCreated listener to update the array of open
  // tabs. Use an onUpdatedListener to make sure the next test doesn't start
  // before the onUpdated events for the create call are finished.
  function testTabCreate() {
    chrome.tabs.onCreated.addListener(function localListener(tab) {
      chrome.test.assertEq(NEW_TAB_URL, tab.pendingUrl);
      tabProps.push({id: tab.id, url: tab.pendingUrl});
      chrome.tabs.onCreated.removeListener(localListener);
    });
    chrome.tabs.onUpdated.addListener(function localListener (
        tabId, changeInfo, tab) {
      if (changeInfo.status === 'complete') {
        chrome.tabs.onUpdated.removeListener(localListener);
        chrome.test.succeed();
      }
    });
    try {
      // Create the tab inactive, so we can activate it later.
      chrome.tabs.create({url: NEW_TAB_URL, active: false});
    } catch (e) {
      chrome.test.fail(e);
    }
  },
  // Test the chrome.tabs.onUpdated listener through the loading cycle.
  function testTabOnUpdatedListener() {
    var newUrl = 'chrome://version/';
    var gotLoading = false;
    chrome.tabs.onUpdated.addListener(function localListener(
        tabId, changeInfo, tab) {
      if (changeInfo.status === 'loading') {
        chrome.test.assertFalse(gotLoading);
        gotLoading = true;
        chrome.test.assertEq(tabProps[1].id, tabId);
        chrome.test.assertEq(newUrl, changeInfo.url);
      } else if (changeInfo.status === 'complete') {
        chrome.test.assertTrue(gotLoading);
        chrome.tabs.onUpdated.removeListener(localListener);
        chrome.test.succeed();
      }
    });
    try {
      chrome.tabs.update(tabProps[1].id, {url: newUrl});
      tabProps[1].url = newUrl;
    } catch(e) {
      chrome.test.fail(e);
    }
  },
  // Check the chrome.tabs.onMoved listener.
  function testTabMove() {
    var expectedId = tabProps[0].id
    chrome.test.listenOnce(chrome.tabs.onMoved,
                           function localListener(tabId, moveInfo) {
      chrome.test.assertEq(expectedId, tabId);
    });
    try {
      chrome.tabs.move(expectedId, {index: -1});
    } catch(e) {
      chrome.test.fail(e);
    }
  },
  // Check the chrome.tabs.onActivated listener.
  function testTabActivated() {
    var tabId = tabProps[1].id;
    chrome.tabs.onActivated.addListener(function localListener(activeInfo) {
      chrome.tabs.onActivated.removeListener(localListener);
      chrome.test.assertEq(tabId, activeInfo.tabId);
      chrome.test.succeed();
    });
    try {
      // Make an existing tab active.
      chrome.tabs.update(tabId, {active: true});
    } catch(e) {
      chrome.test.fail(e);
    }
  },
  // Check the chrome.tabs.onRemoved listener.
  function testTabRemoved() {
    var tabIdToClose = tabProps[1].id;
    chrome.tabs.onRemoved.addListener(function localListener(
        tabId, removeInfo) {
      chrome.tabs.onRemoved.removeListener(localListener);
      chrome.test.assertEq(tabIdToClose, tabId);
      chrome.test.assertFalse(removeInfo.isWindowClosing);
      chrome.test.succeed();
    });
    try {
      // Remove the tab.
      chrome.tabs.remove(tabIdToClose);
    } catch(e) {
      chrome.test.fail(e);
    }
  },
]);
