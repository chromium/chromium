// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var enabled_app, disabled_app, enabled_extension, packaged_app;
var allLaunchTypes = ["OPEN_AS_REGULAR_TAB",
                      "OPEN_AS_PINNED_TAB",
                      "OPEN_AS_WINDOW",
                      "OPEN_FULL_SCREEN"];

function testSetLaunchType(id, type, error, listener) {
  var callListener = function() {
    if (listener)
      listener();
  };

  chrome.test.runWithUserGesture(function() {
    if (!error)
      chrome.management.setLaunchType(id, type, callback(callListener));
    else
      chrome.management.setLaunchType(id, type, callback(callListener, error));
  });
}

function getAvailableLaunchTypes(app) {
  var types = Array();
  if (app.type == "packaged_app") {
    types.push("OPEN_AS_WINDOW");
    return types;
  }

  types.push("OPEN_AS_REGULAR_TAB");
  if (navigator.userAgent.indexOf("Mac") == -1) {
    types.push("OPEN_AS_WINDOW");
  } else {
    types.push("OPEN_AS_PINNED_TAB");
    types.push("OPEN_FULL_SCREEN");
  }

  return types;
}

function verifyAvailableLaunchTypes(expected, actual) {
  assertEq(expected.length, actual.length);
  for (var i = 0; i < expected.length; i++)
    assertTrue(actual.indexOf(expected[i]) != -1);
}

function testSetAllLaunchTypes(app) {
  var availableTypes = getAvailableLaunchTypes(app);

  var setLaunchType = function(i) {
    var setNextLaunchType = function() {
      if (i + 1 < allLaunchTypes.length)
        setLaunchType(i + 1);
    };

    var type = allLaunchTypes[i];
    if (availableTypes.indexOf(type) < 0) {
      testSetLaunchType(app.id,
                        type,
                        "The launch type is not available for this app.",
                        setNextLaunchType);
    } else {
      testSetLaunchType(app.id, type, null, function() {
        chrome.management.get(app.id, function(item) {
          if (navigator.userAgent.indexOf("Mac") != -1) {
            // In the current configuration, with the new bookmark app flow
            // disabled, hosted apps set to open in a window on Mac will open
            // instead in a tab.
            if (item.type != 'packaged_app' && type == 'OPEN_AS_WINDOW')
              type = 'OPEN_AS_REGULAR_TAB';
          }
          assertEq(type, item.launchType);
          setNextLaunchType();
        });
      });
    }
  };

  if (allLaunchTypes.length > 0)
    setLaunchType(0);
}

var tests = [
  function verifyLaunchType() {
    assertTrue(enabled_app.availableLaunchTypes != undefined);
    assertTrue(enabled_app.availableLaunchTypes.indexOf(
        enabled_app.launchType) != -1);

    assertTrue(disabled_app.availableLaunchTypes != undefined);
    assertTrue(disabled_app.availableLaunchTypes.indexOf(
        disabled_app.launchType) != -1);

    assertTrue(packaged_app.availableLaunchTypes != undefined);
    assertTrue(packaged_app.availableLaunchTypes.indexOf(
        packaged_app.launchType) != -1);

    assertEq(undefined, enabled_extension.launchType);
    assertEq(undefined, enabled_extension.availableLaunchTypes);

    verifyAvailableLaunchTypes(getAvailableLaunchTypes(enabled_app),
                               enabled_app.availableLaunchTypes);

    verifyAvailableLaunchTypes(getAvailableLaunchTypes(disabled_app),
                               disabled_app.availableLaunchTypes);

    verifyAvailableLaunchTypes(getAvailableLaunchTypes(packaged_app),
                               packaged_app.availableLaunchTypes);
    succeed();
  },

  function setLaunchTypeWithoutUserGesture() {
    chrome.management.setLaunchType(enabled_app.id, "OPEN_AS_REGULAR_TAB",
        callback(function() {},
            "chrome.management.setLaunchType requires a user gesture."));
  },

  function setEnabledAppLaunchType() {
    testSetAllLaunchTypes(enabled_app);
  },

  function setDisabledAppLaunchType() {
    testSetAllLaunchTypes(disabled_app);
  },

  function setPackagedAppLaunchType() {
    testSetAllLaunchTypes(packaged_app);
  },

  function setExtensionLaunchType() {
    testSetLaunchType(enabled_extension.id, "OPEN_AS_REGULAR_TAB",
        "Extension " + enabled_extension.id + " is not an App.");
  },

  function setNotExistAppLaunchType() {
    testSetLaunchType("abcd", "OPEN_AS_REGULAR_TAB",
        "Failed to find extension with id abcd.");
  }
];

const scriptUrl = '_test_resources/api_test/management/common.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  chrome.management.getAll(callback(function(items) {
    enabled_app = getItemNamed(items, "enabled_app");
    disabled_app = getItemNamed(items, "disabled_app");
    enabled_extension = getItemNamed(items, "enabled_extension");
    packaged_app = getItemNamed(items, "packaged_app");

    chrome.test.runTests(tests);
  }));
});
