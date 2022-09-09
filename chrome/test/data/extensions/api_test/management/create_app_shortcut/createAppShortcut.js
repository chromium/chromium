// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function testCreateAppShortcut(id, error) {
  chrome.test.runWithUserGesture(function() {
    if (!error)
      chrome.management.createAppShortcut(id, callback(function() {}));
    else
      chrome.management.createAppShortcut(id, callback(function() {}, error));
  });
}

var enabled_app_id, disabled_app_id, enabled_extension_id, packaged_app_id;
var isMac = /Mac/.test(navigator.platform);
var ONLY_PACKAGED_APP_MAC =
    "Shortcuts can only be created for new-style packaged apps on Mac.";

var tests = [
  function createEnabledAppShortcutWithoutUserGesture() {
    chrome.management.createAppShortcut(enabled_app_id, callback(function() {},
        "chrome.management.createAppShortcut requires a user gesture."));
  },

  function createEnabledAppShortcut() {
    testCreateAppShortcut(enabled_app_id, isMac? ONLY_PACKAGED_APP_MAC : null);
  },

  function createDisabledAppShortcut() {
    testCreateAppShortcut(disabled_app_id,
        isMac? ONLY_PACKAGED_APP_MAC : null);
  },

  function createPackagedAppShortcut() {
    testCreateAppShortcut(packaged_app_id);
  },

  function createExtensionShortcut() {
    testCreateAppShortcut(enabled_extension_id,
        "Extension " + enabled_extension_id + " is not an App.");
  },

  function createNotExistAppShortcut() {
    testCreateAppShortcut("abcd", "Failed to find extension with id abcd.");
  }
];

const scriptUrl = '_test_resources/api_test/management/common.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  chrome.management.getAll(callback(function(items) {
    enabled_app_id = getItemNamed(items, "enabled_app").id;
    disabled_app_id = getItemNamed(items, "disabled_app").id;
    enabled_extension_id = getItemNamed(items, "enabled_extension").id;
    packaged_app_id = getItemNamed(items, "packaged_app").id;

    chrome.test.runTests(tests);
  }));
});
