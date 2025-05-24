// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabIds = [];

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function setUp() {
    chrome.tabs.create({"url": pageUrl("a")}, pass(function(tab) {
      tabIds.push(tab.id);
    }));
    chrome.tabs.create({"url": pageUrl("b")}, pass(function(tab) {
      tabIds.push(tab.id);
    }));
    chrome.tabs.create({"url": pageUrl("c")}, pass(function(tab) {
      tabIds.push(tab.id);
    }));
    },

  function testBasicSetup() {
    waitForAllTabs(pass(function() {
      chrome.tabs.get(tabIds[0], pass(function(tab) {
        assertEq(pageUrl("a"), tab.url);
      }));
      chrome.tabs.get(tabIds[1], pass(function(tab) {
        assertEq(pageUrl("b"), tab.url);
      }));
      chrome.tabs.get(tabIds[2], pass(function(tab) {
        assertEq(pageUrl("c"), tab.url);
      }));
    }));
  },

  function testUpdatingDefaultTabViaUndefined() {
    chrome.tabs.update(
      tabIds[1],
      {"selected": true},
      pass(function(tab) {
        chrome.tabs.update(
          undefined,
          {"url": pageUrl("d")},
          pass(function(tab) {
            waitForAllTabs(pass(function() {
              chrome.tabs.get(
                tabIds[1],
                pass(function(tab) {
                  assertEq(pageUrl("d"), tab.url);
                }));
            }));
          }));
      }));
  },

  function testUpdatingDefaultTabViaNull() {
    chrome.tabs.update(
      tabIds[2],
      {"selected": true},
      pass(function(tab) {
        chrome.tabs.update(
          null,
          {"url": pageUrl("e")},
          pass(function(tab) {
            waitForAllTabs(pass(function() {
            chrome.tabs.get(
              tabIds[2],
              pass(function(tab) {
                assertEq(pageUrl("e"), tab.url);
              }));
            }));
          }));
      }));
  },

  function testUpdatingWithPermissionReturnsTabInfo() {
    chrome.tabs.update(
      undefined, {"url": pageUrl("neutrinos")}, pass(function(tab) {
        assertEq(pageUrl("neutrinos"), tab.pendingUrl);
    }));
  },

  function testUpdatingToUrlThatWillBeRejectedDuringNavigation() {
    // We have a maximum length on URLs that we support. Today, this is
    // 2 * 1024 * 1024. Pick a URL significantly longer than that.
    const url = 'http://example.com/' + 'a'.repeat(10 * 1024 * 1024);

    // Try to update the tab to that URL. This will result in the navigation
    // failing, but shouldn't result in any browser crashes.
    // See https://crbug.com/373838227.
    chrome.tabs.update(
        tabIds[2], {url},
        () => {
          chrome.test.assertLastError('Navigation rejected.');
          chrome.test.succeed();
        });
  },
])});
