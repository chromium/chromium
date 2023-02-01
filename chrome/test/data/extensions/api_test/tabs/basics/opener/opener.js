// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var firstTabId;
var secondTabId;
var thirdTabId;

const scriptUrl = '_test_resources/api_test/tabs/basics/tabs_util.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
chrome.test.runTests([
  function init() {
    chrome.tabs.create({index:1, active:false}, pass(function(tab) {
      secondTabId = tab.id;
      assertFalse(tab.active);
      assertEq(1, tab.index);
      chrome.tabs.query(
          {windowId: chrome.windows.WINDOW_ID_CURRENT, index:0},
          pass(function(tabs) {
        assertEq(1, tabs.length);
        assertEq(0, tabs[0].index);
        firstTabId = tabs[0].id;
      }));
    }));
  },

  function createWithOpener() {
    chrome.tabs.create(
        {openerTabId: firstTabId, active: true},
        pass(function(tab) {
      assertEq(firstTabId, tab.openerTabId);
      assertTrue(tab.active);
      assertEq(2, tab.index);
      thirdTabId = tab.id;
    }));
  },

  function closeOpener() {
    chrome.tabs.remove(thirdTabId, pass(function() {
      chrome.tabs.get(firstTabId, pass(function(tab) {
        assertTrue(tab.active);
      }));
    }));
  },

  function updateOpener() {
    chrome.tabs.create({active: true}, pass(function(tab1) {
      thirdTabId = tab1.id;
      assertTrue(tab1.active);
      assertFalse("openerTabId" in tab1);
      chrome.tabs.update(
          tab1.id, {openerTabId: firstTabId},
          pass(function(tab2) {
        assertEq(firstTabId, tab2.openerTabId);
      }));
    }));
  },

  function closeOpenerAgain() {
    chrome.tabs.remove(thirdTabId, pass(function() {
      chrome.tabs.get(firstTabId, pass(function(tab) {
        assertTrue(tab.active);
      }));
    }));
  }
])});
