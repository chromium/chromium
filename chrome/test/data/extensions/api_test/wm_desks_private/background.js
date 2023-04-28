// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Basic API tests for the wmDesksPrivate API.
chrome.test.runTests([
  async function testGetDeskTemplateJson() {
    await chrome.test.assertPromiseRejects(
      chrome.wmDesksPrivate.getDeskTemplateJson(
      // Get desk template JSON with an invalid UUID.
        'invalid-uuid'), 'Error: InvalidIdError');
    chrome.test.succeed();
  },

  // Tests setting window to show up on all desks.
  async function testSetToAllDeskWindowWithValidID() {
    // Create a new window.
    // Note: create a dummy window first to avoid test flakiness. In
    // test_ash_chrome binary, creating new window sometimes fail to be
    // populated into root window list. The issue doesn't exist in actual
    // ash-chrome binary,
    await chrome.windows.create();
    const window = await chrome.windows.create();
    await chrome.wmDesksPrivate.setWindowProperties(window.tabs[0].windowId,
      { allDesks: true });

    chrome.test.succeed();
  },

  // Tests reverting setting window to show up on all desks.
  async function testUnsetToAllDeskWindowWithValidID() {
    // Create a new window.
    const window = await chrome.windows.create();
    await chrome.wmDesksPrivate.setWindowProperties(window.tabs[0].windowId,
      { allDesks: false });
    chrome.test.succeed();
  },

  // Tests SetToAllDeskWindow with invalid `window_id`.
  async function testSetToAllDeskWindowWithInvalidID() {
    // Launch invalid template Uuid.
    await chrome.test.assertPromiseRejects(
      chrome.wmDesksPrivate.setWindowProperties(1234, { allDesks: true }),
      "Error: ResourceNotFoundError");
    chrome.test.succeed();

  },

  // Tests UnsetAllDeskWindow with invalid `window_id`.
  async function testUnsetAllDeskWindowWithInvalidID() {
    // Launch invalid template Uuid.
    await chrome.test.assertPromiseRejects(
      chrome.wmDesksPrivate.setWindowProperties(1234, { allDesks: false }),
      "Error: ResourceNotFoundError");
    chrome.test.succeed();
  },

  async function testGetSavedDesks() {
    const saved_desks = await chrome.wmDesksPrivate.getSavedDesks();
    chrome.test.assertEq(0, saved_desks.length);
    chrome.test.succeed();
  }
]);
