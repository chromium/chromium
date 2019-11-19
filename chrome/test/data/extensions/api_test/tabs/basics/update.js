// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabIds = [];

chrome.test.runTests([
  function setUp() {
    chrome.tabs.create({"url": pageUrl("a")}, function(tab) {
      tabIds.push(tab.id);
    });
    chrome.tabs.create({"url": pageUrl("b")}, function(tab) {
      tabIds.push(tab.id);
    });
    chrome.tabs.create({"url": pageUrl("c")}, function(tab) {
      tabIds.push(tab.id);
    });
    chrome.windows.create({"url": pageUrl("xxx")}, pass(function(tab) {}));
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
  }
]);
