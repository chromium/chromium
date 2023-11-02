// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var EXPECTED_ERROR = 'chrome.management.uninstall requires a user gesture.';

function uninstall(name) {
  var expected_id;
  listenOnce(chrome.management.onUninstalled, function(id) {
    assertEq(expected_id, id);
  });

  chrome.management.getAll(callback(function(items) {
    var old_count = items.length;
    var item = getItemNamed(items, name);
    expected_id = item.id;
    chrome.test.runWithUserGesture(function() {
      chrome.management.uninstall(item.id, callback(function() {
        chrome.management.getAll(callback(function(items2) {
          assertEq(old_count - 1, items2.length);
          for (var i = 0; i < items2.length; i++) {
            assertFalse(items2[i].name == name);
          }
        }));
      }));
    });
  }));
}

function uninstallWithoutUserGesture(name) {
  chrome.management.getAll(callback(function(items) {
    var old_count = items.length;
    var item = getItemNamed(items, name);
    chrome.management.uninstall(item.id, callback(function() {
      chrome.management.getAll(callback(function(items2) {
        assertEq(old_count, items2.length);
      }));
    }, EXPECTED_ERROR));
  }));
}

var tests = [

  function uninstallEnabledAppWithoutUserGesture() {
    uninstallWithoutUserGesture("enabled_app");
  },

  function uninstallEnabledApp() {
    uninstall("enabled_app");
  },

  function uninstallDisabledApp() {
    uninstall("disabled_app");
  },

  function uninstallEnabledExtension() {
    uninstall("enabled_extension");
  },

  function uninstallDisabledExtension() {
    uninstall("disabled_extension");
  }
];

const scriptUrl = '_test_resources/api_test/management/common.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  chrome.test.runTests(tests);
});
