// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let enabledApp;
let disabledApp;
let enabledExtension;
let packagedApp;
const allLaunchTypes = [
  'OPEN_AS_REGULAR_TAB',
  'OPEN_AS_PINNED_TAB',
  'OPEN_AS_WINDOW',
  'OPEN_FULL_SCREEN',
];

function testSetLaunchType(id, type, error, listener) {
  const callListener = function() {
    if (listener) {
      listener();
    }
  };

  chrome.test.runWithUserGesture(function() {
    if (!error) {
      chrome.management.setLaunchType(id, type, callback(callListener));
    } else {
      chrome.management.setLaunchType(id, type, callback(callListener, error));
    }
  });
}

function getAvailableLaunchTypes(app) {
  const types = [];
  if (app.type == 'packaged_app') {
    types.push('OPEN_AS_WINDOW');
    return types;
  }

  types.push('OPEN_AS_REGULAR_TAB');
  if (navigator.userAgent.indexOf('Mac') == -1) {
    types.push('OPEN_AS_WINDOW');
  } else {
    types.push('OPEN_AS_PINNED_TAB');
    types.push('OPEN_FULL_SCREEN');
  }

  return types;
}

function verifyAvailableLaunchTypes(expected, actual) {
  assertEq(expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    assertTrue(actual.indexOf(expected[i]) != -1);
  }
}

function testSetAllLaunchTypes(app) {
  const availableTypes = getAvailableLaunchTypes(app);

  const setLaunchType = function(i) {
    const setNextLaunchType = function() {
      if (i + 1 < allLaunchTypes.length) {
        setLaunchType(i + 1);
      }
    };

    let type = allLaunchTypes[i];
    if (availableTypes.indexOf(type) < 0) {
      testSetLaunchType(
          app.id, type, 'The launch type is not available for this app.',
          setNextLaunchType);
    } else {
      testSetLaunchType(app.id, type, null, function() {
        chrome.management.get(app.id, function(item) {
          if (navigator.userAgent.indexOf('Mac') != -1) {
            // In the current configuration, with the new bookmark app flow
            // disabled, hosted apps set to open in a window on Mac will open
            // instead in a tab.
            if (item.type != 'packaged_app' && type == 'OPEN_AS_WINDOW') {
              type = 'OPEN_AS_REGULAR_TAB';
            }
          }
          assertEq(type, item.launchType);
          setNextLaunchType();
        });
      });
    }
  };

  if (allLaunchTypes.length > 0) {
    setLaunchType(0);
  }
}

const tests = [
  function verifyLaunchType() {
    assertTrue(enabledApp.availableLaunchTypes != undefined);
    assertTrue(
        enabledApp.availableLaunchTypes.indexOf(enabledApp.launchType) != -1);

    assertTrue(disabledApp.availableLaunchTypes != undefined);
    assertTrue(
        disabledApp.availableLaunchTypes.indexOf(disabledApp.launchType) != -1);

    assertTrue(packagedApp.availableLaunchTypes != undefined);
    assertTrue(
        packagedApp.availableLaunchTypes.indexOf(packagedApp.launchType) != -1);

    assertEq(undefined, enabledExtension.launchType);
    assertEq(undefined, enabledExtension.availableLaunchTypes);

    verifyAvailableLaunchTypes(
        getAvailableLaunchTypes(enabledApp), enabledApp.availableLaunchTypes);

    verifyAvailableLaunchTypes(
        getAvailableLaunchTypes(disabledApp), disabledApp.availableLaunchTypes);

    verifyAvailableLaunchTypes(
        getAvailableLaunchTypes(packagedApp), packagedApp.availableLaunchTypes);
    succeed();
  },

  function setLaunchTypeWithoutUserGesture() {
    chrome.management.setLaunchType(
        enabledApp.id, 'OPEN_AS_REGULAR_TAB', callback(function() {
        }, 'chrome.management.setLaunchType requires a user gesture.'));
  },

  function setEnabledAppLaunchType() {
    testSetAllLaunchTypes(enabledApp);
  },

  function setDisabledAppLaunchType() {
    testSetAllLaunchTypes(disabledApp);
  },

  function setPackagedAppLaunchType() {
    testSetAllLaunchTypes(packagedApp);
  },

  function setExtensionLaunchType() {
    testSetLaunchType(
        enabledExtension.id, 'OPEN_AS_REGULAR_TAB',
        `Extension ${enabledExtension.id} is not an App.`);
  },

  function setNotExistAppLaunchType() {
    testSetLaunchType(
        'abcd', 'OPEN_AS_REGULAR_TAB',
        'Failed to find extension with id abcd.');
  },
];

const SCRIPT_URL = '_test_resources/api_test/management/common.js';
const loadScript = chrome.test.loadScript(SCRIPT_URL);

loadScript.then(async function() {
  chrome.management.getAll(callback(function(items) {
    enabledApp = getItemNamed(items, 'enabled_app');
    disabledApp = getItemNamed(items, 'disabled_app');
    enabledExtension = getItemNamed(items, 'enabled_extension');
    packagedApp = getItemNamed(items, 'packaged_app');

    chrome.test.runTests(tests);
  }));
});
