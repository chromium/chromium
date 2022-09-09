// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regex for UUID.
const uuidRegex = new RegExp('' +
  /^[0-9a-fA-F]{8}\b-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{4}\b/.source +
  /-[0-9a-fA-F]{4}\b-[0-9a-fA-F]{12}$/.source);
var templateUuid;

// Basic browser tests for the wmDesksPrivate API.
chrome.test.runTests([
  async function testGetDeskTemplateJson() {
    await chrome.test.assertPromiseRejects(
      chrome.wmDesksPrivate.getDeskTemplateJson(
      // Get desk template JSON with an invalid UUID.
        'invalid-uuid'), 'Error: Invalid template UUID.');
    chrome.test.succeed();
  },

  // Tests List all desks.
  async function testGetAllDesks() {
    await chrome.wmDesksPrivate.launchDesk({ deskName: "test" });
    // Launch invalid template Uuid.
    const allDesks = await chrome.wmDesksPrivate.getAllDesks();
    chrome.test.assertEq(2, allDesks.length);
    chrome.test.succeed();
  },

  // Tests launching empty desk with a desk name.
  async function testLaunchEmptyDeskWithNameAndRemoveDesk() {
    // Launch empty desk with `deskName`.
    const deskUuid = await
      chrome.wmDesksPrivate.launchDesk({ deskName: "test" });
    // Desk uuid should be returned.
    chrome.test.assertTrue(uuidRegex.test(deskUuid));

    // Clean Up.
    await chrome.wmDesksPrivate.removeDesk(deskUuid, { combineDesks: false });
    chrome.test.succeed();
  },

  // Tests setting window to show up on all desks.
  async function testSetToAllDeskWindowWithValidID() {
    // Create a new window.
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
      "Error: The window cannot be found.");
    chrome.test.succeed();

  },

  // Tests UnsetAllDeskWindow with invalid `window_id`.
  async function testUnsetAllDeskWindowWithInvalidID() {
    // Launch invalid template Uuid.
    await chrome.test.assertPromiseRejects(
      chrome.wmDesksPrivate.setWindowProperties(1234, { allDesks: false }),
      "Error: The window cannot be found.");
    chrome.test.succeed();
  },

  // Tests save an active desk to library.
  async function testSaveActiveDesk() {
    const savedDesk = await chrome.wmDesksPrivate.saveActiveDesk();
    chrome.test.assertTrue(uuidRegex.test(savedDesk.deskUuid));
    chrome.test.assertTrue(savedDesk.hasOwnProperty('deskName'));
    chrome.test.succeed();
  },

  // Tests delete a saved desk from library.
  async function testDeleteSavedDesk() {
    const savedDesk = await chrome.wmDesksPrivate.saveActiveDesk();
    await chrome.wmDesksPrivate.deleteSavedDesk(savedDesk.deskUuid);
    chrome.test.succeed();
  },

  // Tests recall a saved desk from library.
  async function testRecallSavedDesk() {
    const savedDesk = await chrome.wmDesksPrivate.saveActiveDesk();
    const newDeskUuid = await chrome.wmDesksPrivate.recallSavedDesk(
      savedDesk.deskUuid);
    chrome.test.assertTrue(uuidRegex.test(newDeskUuid));
    chrome.test.succeed();
  },
]);
