// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const callback = chrome.test.callback;

// The names of the extensions to operate on, from their manifest.json files.
const ENABLED_NAME = 'enabled_extension';
const DISABLED_NAME = 'disabled_extension';
const UNINSTALL_NAME = 'enabled_extension';

const EXPECTED_ERROR = 'Extension * cannot be modified by user.';

// Given a list of extension |items|, finds the one with the given |name|.
function findByName(items, name) {
  chrome.test.assertEq(8, items.length);
  let item;
  for (let i = 0; i < items.length; i++) {
    item = items[i];
    if (item.name == name) {
      break;
    }
  }
  if (name != item.name) {
    chrome.test.fail(`Couldn\'t find installed extension ${name}`);
  }
  return item;
}

// Verifies that the extension with the given |id| has the expected |enabled|
// status.
function checkEnabled(id, enabled) {
  chrome.management.get(id, callback(function(result) {
                          chrome.test.assertEq(enabled, result.enabled);
                        }));
}

const allowedTests = [
  // Tests disabling an extension, expecting it to succeed.
  function allowedDisable() {
    chrome.management.getAll(callback(function(items) {
      const item = findByName(items, ENABLED_NAME);
      chrome.test.assertEq(true, item.mayDisable);
      chrome.test.assertEq(true, item.enabled);

      const id = item.id;
      chrome.management.setEnabled(id, false, callback(function() {
                                     chrome.management.get(
                                         id, callback(function(same_extension) {
                                           chrome.test.assertEq(
                                               false, same_extension.enabled);
                                         }));
                                   }));
    }));
  },

  // Tests enabling an extension, expecting it to succeed.
  function allowedEnable() {
    chrome.management.getAll(callback(function(items) {
      const item = findByName(items, DISABLED_NAME);
      chrome.test.assertEq(true, item.mayDisable);
      chrome.test.assertEq(false, item.enabled);

      const id = item.id;
      chrome.management.setEnabled(id, true, callback(function() {
                                     chrome.management.get(
                                         id, callback(function(same_extension) {
                                           chrome.test.assertEq(
                                               true, same_extension.enabled);
                                         }));
                                   }));
    }));
  },

  // Tests uninstalling an extension, expecting it to succeed.
  function allowedUninstall() {
    chrome.management.getAll(callback(function(items) {
      const item = findByName(items, UNINSTALL_NAME);
      chrome.test.assertEq(true, item.mayDisable);

      const id = item.id;
      chrome.test.runWithUserGesture(function() {
        chrome.management.uninstall(id, callback(function() {
                                      chrome.test.assertNoLastError();
                                      // The calling api test will verify that
                                      // the item was uninstalled.
                                    }));
      });
    }));
  },
];

const prohibitedTests = [
  // Tests disabling an extension, expecting it to fail.
  function prohibitedDisable() {
    chrome.management.getAll(callback(function(items) {
      const item = findByName(items, ENABLED_NAME);
      chrome.test.assertEq(false, item.mayDisable);
      chrome.test.assertEq(true, item.enabled);

      const id = item.id;
      const expectedError = EXPECTED_ERROR.replace('*', id);
      chrome.management.setEnabled(id, false, callback(function() {
                                     checkEnabled(id, true);
                                   }, expectedError));
    }));
  },

  // Tests enabling an extension, expecting it to fail.
  function prohibitedEnable() {
    chrome.management.getAll(callback(function(items) {
      const item = findByName(items, DISABLED_NAME);
      chrome.test.assertEq(false, item.mayDisable);
      chrome.test.assertEq(false, item.enabled);

      const id = item.id;
      const expectedError = EXPECTED_ERROR.replace('*', id);
      chrome.management.setEnabled(id, true, callback(function() {
                                     checkEnabled(id, false);
                                   }, expectedError));
    }));
  },

  // Tests uninstalling an extension, expecting it to fail.
  function prohibitedUninstall() {
    chrome.management.getAll(callback(function(items) {
      const item = findByName(items, UNINSTALL_NAME);
      chrome.test.assertEq(false, item.mayDisable);

      const id = item.id;
      const expectedError = EXPECTED_ERROR.replace('*', id);
      chrome.test.runWithUserGesture(function() {
        chrome.management.uninstall(id, callback(function() {
                                      checkEnabled(id, true);
                                    }, expectedError));
      });
    }));
  },
];

const testSuites = {
  runAllowedTests: allowedTests,
  runProhibitedTests: prohibitedTests,
};

chrome.test.getConfig(function(config) {
  const suite = testSuites[config.customArg];
  if (config.customArg in testSuites) {
    chrome.test.runTests(testSuites[config.customArg]);
  } else {
    chrome.test.fail('Invalid test suite');
  }
});
