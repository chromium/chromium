// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var templateUuid;

// Basic browser tests for the wmDesksPrivate API.
chrome.test.runTests([
  // Test launch empty desk with a desk name.
  function testLaunchEmptyDeskWithName() {
    // Launch empty desk with `deskName`
    chrome.wmDesksPrivate.launchDesk({ deskName: "test" },
      chrome.test.callbackPass(function (result) {
        // Desk uuid should be returned.
        chrome.test.assertEq(typeof result, 'string');
      }));
  },

  // Test set window to show up on all desks.
  function testSetToAllDeskWindowWithValidID() {
    // Launch a new desk.
    chrome.wmDesksPrivate.launchDesk({ deskName: "test" }, () => { });
    // Create a new window.
    var windowId;
    chrome.windows.create((window) => {
      windowId = window.tabs[0].windowId;
      chrome.wmDesksPrivate.setWindowProperties(windowId, { allDesks: true },
        chrome.test.callbackPass())
    });
  },

  // Test revert setting window to show up on all desks.
  function testUnsetToAllDeskWindowWithValidID() {
    // Launch a new desk.
    chrome.wmDesksPrivate.launchDesk({ deskName: "test" }, () => { });
    // Create a new window.
    var windowId;
    chrome.windows.create((window) => {
      windowId = window.tabs[0].windowId;
      chrome.wmDesksPrivate.setWindowProperties(windowId, { allDesks: false },
        chrome.test.callbackPass())
    });
  },

  // Test SetToAllDeskWindow invalid `window_id`.
  function testSetToAllDeskWindowWithInvalidID() {
    // Launch invalid template Uuid
    chrome.wmDesksPrivate.setWindowProperties(1234, { allDesks: true },
      // Launch desk fail with invalid templateUuid
      chrome.test.callbackFail("The window cannot be found."));
  },

  // Test UnsetAllDeskWindow invalid `window_id`.
  function testUnsetAllDeskWindowWithInvalidID() {
    // Launch invalid template Uuid
    chrome.wmDesksPrivate.setWindowProperties(1234, { allDesks: false },
      // Launch desk fail with invalid templateUuid
      chrome.test.callbackFail("The window cannot be found."));
    }
]);
