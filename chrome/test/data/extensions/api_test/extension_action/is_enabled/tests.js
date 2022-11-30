// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { openTab } from '/_test_resources/test_util/tabs_util.js';
let tab1Id;
let tab2Id;
chrome.test.getConfig(async config => {
  const port = config.testServer.port;
  chrome.test.runTests([
    // Open two tabs before proceeding to the tests.
    async function setUp() {
      const tab1 = await openTab(
        `http://localhost:${port}/extensions/test_file.html`);
      tab1Id = tab1.id;
      const tab2 = await openTab(
        `http://localhost:${port}/extensions/test_file.html`);
      tab2Id = tab2.id;
      chrome.test.succeed();
    },
    // Tests that disabling by default or enabled by default causes the
    // default and all tabs to report as disabled.
    async function testDefaultSetting() {
      await chrome.action.disable();
      chrome.test.assertFalse(await chrome.action.isEnabled());
      chrome.test.assertFalse(await chrome.action.isEnabled(tab1Id));
      chrome.test.assertFalse(await chrome.action.isEnabled(tab2Id));

      await chrome.action.enable();
      chrome.test.assertTrue(await chrome.action.isEnabled());
      chrome.test.assertTrue(await chrome.action.isEnabled(tab1Id));
      chrome.test.assertTrue(await chrome.action.isEnabled(tab2Id));
      chrome.test.succeed();
    },
    // Tests that enabling a specific tab only enables that tab and
    // not by default.
    async function testTabSpecificDisableSetting() {
      await chrome.action.disable();
      chrome.test.assertFalse(await chrome.action.isEnabled());

      await chrome.action.enable(tab1Id);
      chrome.test.assertTrue(await chrome.action.isEnabled(tab1Id));
      chrome.test.assertFalse(await chrome.action.isEnabled(tab2Id));
      chrome.test.succeed();
    },
    // Tests that disabling a specific tab only disables that tab and
    // not by default.
    async function testTabSpecificEnableSetting() {
      await chrome.action.enable();
      chrome.test.assertTrue(await chrome.action.isEnabled());

      await chrome.action.disable(tab1Id);
      chrome.test.assertFalse(await chrome.action.isEnabled(tab1Id));
      chrome.test.assertTrue(await chrome.action.isEnabled(tab2Id));
      chrome.test.succeed();
    }
  ]);
});
