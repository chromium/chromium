// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testCreateAppShortcut(id, error) {
  chrome.test.runWithUserGesture(function() {
    if (!error) {
      chrome.management.createAppShortcut(id, callback(function() {}));
    } else {
      chrome.management.createAppShortcut(id, callback(function() {}, error));
    }
  });
}

let enabledAppId;
let disabledAppId;
let enabledExtensionId;
let packagedAppId;
const isMac = /Mac/.test(navigator.platform);
const ONLY_PACKAGED_APP_MAC =
    'Shortcuts can only be created for new-style packaged apps on Mac.';

const tests = [
  function createEnabledAppShortcutWithoutUserGesture() {
    chrome.management.createAppShortcut(
        enabledAppId, callback(function() {
        }, 'chrome.management.createAppShortcut requires a user gesture.'));
  },

  function createEnabledAppShortcut() {
    testCreateAppShortcut(enabledAppId, isMac ? ONLY_PACKAGED_APP_MAC : null);
  },

  function createDisabledAppShortcut() {
    testCreateAppShortcut(disabledAppId, isMac ? ONLY_PACKAGED_APP_MAC : null);
  },

  function createPackagedAppShortcut() {
    testCreateAppShortcut(packagedAppId);
  },

  function createExtensionShortcut() {
    testCreateAppShortcut(
        enabledExtensionId, `Extension ${enabledExtensionId} is not an App.`);
  },

  function createNotExistAppShortcut() {
    testCreateAppShortcut('abcd', 'Failed to find extension with id abcd.');
  },
];

const SCRIPT_URL = '_test_resources/api_test/management/common.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.management.getAll(callback(function(items) {
    enabledAppId = getItemNamed(items, 'enabled_app').id;
    disabledAppId = getItemNamed(items, 'disabled_app').id;
    enabledExtensionId = getItemNamed(items, 'enabled_extension').id;
    packagedAppId = getItemNamed(items, 'packaged_app').id;

    chrome.test.runTests(tests);
  }));
});
