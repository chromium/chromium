// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var templateUuid;

// Basic browser tests for the wmDesksPrivate API.
chrome.test.runTests([
  // Tests the entire work flow for template: capturing the active desk, saving
  // it as the desk template, listing it and deleting the template.
  // For now it only contains the part of capturing active desk. The others will
  // be added in following CLs.
  function testTemplateFlow() {
    chrome.wmDesksPrivate.captureActiveDeskAndSaveTemplate(
        chrome.test.callbackPass(function(deskTemplate) {
          chrome.test.assertEq(typeof deskTemplate, 'object');
          chrome.test.assertTrue(deskTemplate.hasOwnProperty('templateUuid'));
          templateUuid = deskTemplate.templateUuid;
          chrome.test.assertTrue(deskTemplate.hasOwnProperty('templateName'));
        }));
  },

  // Test launch empty desk with a desk name.
  function testLaunchEmptyDeskWithName() {
    // Launch empty desk with `deskName`
    chrome.wmDesksPrivate.launchDesk({ deskName: "test" },
      chrome.test.callbackPass(function (result) {
        // Desk uuid should be returned.
        chrome.test.assertEq(typeof result, 'string');
      }));
  },

  // Test launch a desk template to a new desk.
  function testLaunchDeskTemplate() {
    // Launch template to a new desk
    chrome.wmDesksPrivate.launchDesk({ templateUuid: templateUuid },
      chrome.test.callbackPass(function (result) {
        // Desk uuid should be returned.
        chrome.test.assertEq(typeof result, 'string');
      }));
  },

  // Test launch with invalid template ID.
  function testLaunchDeskTemplateWithInvalidID() {
    // Launch invalid template Uuid
    chrome.wmDesksPrivate.launchDesk({ templateUuid: "abcd" },
      // Launch desk fail with invalid templateUuid
      chrome.test.callbackFail("Storage error."));
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
