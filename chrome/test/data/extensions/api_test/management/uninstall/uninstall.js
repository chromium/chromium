// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const EXPECTED_ERROR = 'chrome.management.uninstall requires a user gesture.';

function uninstall(name) {
  let expectedId;
  listenOnce(chrome.management.onUninstalled, function(id) {
    assertEq(expectedId, id);
  });

  chrome.management.getAll(callback(function(items) {
    const oldCount = items.length;
    const item = getItemNamed(items, name);
    expectedId = item.id;
    chrome.test.runWithUserGesture(function() {
      chrome.management.uninstall(
          item.id, callback(function() {
            chrome.management.getAll(callback(function(items2) {
              assertEq(oldCount - 1, items2.length);
              for (let i = 0; i < items2.length; i++) {
                assertFalse(items2[i].name == name);
              }
            }));
          }));
    });
  }));
}

function uninstallWithoutUserGesture(name) {
  chrome.management.getAll(callback(function(items) {
    const oldCount = items.length;
    const item = getItemNamed(items, name);
    chrome.management.uninstall(
        item.id, callback(function() {
          chrome.management.getAll(callback(function(items2) {
            assertEq(oldCount, items2.length);
          }));
        }, EXPECTED_ERROR));
  }));
}

const tests = [

  function uninstallEnabledAppWithoutUserGesture() {
    uninstallWithoutUserGesture('enabled_app');
  },

  function uninstallEnabledApp() {
    uninstall('enabled_app');
  },

  function uninstallDisabledApp() {
    uninstall('disabled_app');
  },

  function uninstallEnabledExtension() {
    uninstall('enabled_extension');
  },

  function uninstallDisabledExtension() {
    uninstall('disabled_extension');
  },
];

const SCRIPT_URL = '_test_resources/api_test/management/common.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.test.runTests(tests);
});
