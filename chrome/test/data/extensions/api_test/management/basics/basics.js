// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function checkIcon(item, size, path) {
  var icons = item.icons;
  for (var i = 0; i < icons.length; i++) {
    var icon = icons[i];
    if (icon.size == size) {
      var expected_url =
          "chrome://extension-icon/" + item.id + "/" + size + "/0";
      assertEq(expected_url, icon.url);
      return;
    }
  }
  fail("didn't find icon of size " + size + " at path " + path);
}

function checkPermission(item, perm) {
  var permissions = item.permissions;
  console.log("permissions for " + item.name);
  for (var i = 0; i < permissions.length; i++) {
    var permission = permissions[i];
    console.log(" " + permission);
    if (permission == perm) {
      assertEq(perm, permission);
      return;
    }
  }
  fail("didn't find permission " + perm);
}

function checkHostPermission(item, perm) {
  var permissions = item.hostPermissions;
  for (var i = 0; i < permissions.length; i++) {
    var permission = permissions[i];
    if (permission == perm) {
      assertEq(perm, permission);
      return;
    }
  }
  fail("didn't find permission " + perm);
}

var tests = [
  function simple() {
    chrome.management.getAll(callback(function(items) {
      chrome.test.assertEq(12, items.length);

      checkItemInList(items, "Extension Management API Test", true,
                      "extension");
      checkItemInList(items, "description", true, "extension",
          { "description": "a short description" });
      checkItemInList(items, "short_name", true, "extension",
          { "shortName": "a short name" });
      checkItemInList(items, "enabled_app", true, "hosted_app",
          { "appLaunchUrl": "http://www.google.com/",
            "offlineEnabled": true,
            "updateUrl": "http://example.com/update.xml" });
      checkItemInList(items, "disabled_app", false, "hosted_app",
          { "disabledReason": "unknown" });
      checkItemInList(items, "enabled_extension", true, "extension",
          { "homepageUrl": "http://example.com/" });
      checkItemInList(items, "disabled_extension", false, "extension",
          { "optionsUrl": "chrome-extension://<ID>/pages/options.html",
            "disabledReason": "unknown" });
      checkItemInList(items, "description", true, "extension",
          { "installType": "development" });
      checkItemInList(items, "internal_extension", true, "extension",
          { "installType": "normal" });
      checkItemInList(items, "external_extension", true, "extension",
          { "installType": "sideload" });
      checkItemInList(items, "admin_extension", true, "extension",
          { "installType": "admin" });
      checkItemInList(items, "version_name", true, "extension",
          { "versionName": "0.1 beta" });

      // Check that we got the icons correctly
      var extension = getItemNamed(items, "enabled_extension");
      assertEq(3, extension.icons.length);
      checkIcon(extension, 128, "icon_128.png");
      checkIcon(extension, 48, "icon_48.png");
      checkIcon(extension, 16, "icon_16.png");

      // Check that we can retrieve this extension by ID.
      chrome.management.get(extension.id, callback(function(same_extension) {
        checkItem(same_extension, extension.name, extension.enabled,
                  extension.type, extension.additional_properties);
      }));

      // Check that we have a permission defined.
      var testExtension = getItemNamed(items, "Extension Management API Test");
      checkPermission(testExtension, "management");

      var permExtension = getItemNamed(items, "permissions");
      checkPermission(permExtension, "unlimitedStorage");
      checkPermission(permExtension, "notifications");
      checkHostPermission(permExtension, "http://*/*");
    }));
  },

  function permissionWarnings() {
    var manifest_str =
        `{
           "name": "Hello World!",
           "manifest_version": 2,
           "version": "1.0",
           "permissions": ["http://api.flickr.com/", "bookmarks", "geolocation",
                           "history", "tabs"],
           "content_scripts": [
             {"js": ["script.js"], "matches": ["http://*.flickr.com/*"]}
           ]
         }`;

    chrome.management.getPermissionWarningsByManifest(
        manifest_str, callback(function(warnings) {
      // Warning for "tabs" is suppressed by "history" permission.
      chrome.test.assertEq(4, warnings.length);
      chrome.test.assertTrue(warnings.indexOf(
        "Read and change your data on all flickr.com sites and api.flickr.com")
        != -1);
      chrome.test.assertTrue(warnings.indexOf(
        "Read and change your bookmarks") != -1);
      chrome.test.assertTrue(warnings.indexOf(
        "Detect your physical location") != -1);
      chrome.test.assertTrue(
          warnings.indexOf(
              "Read and change your browsing history on all your signed-in " +
              "devices") != -1);
    }));

    chrome.management.getAll(callback(function(items) {
      var extension = getItemNamed(items, "Extension Management API Test");
      chrome.management.getPermissionWarningsById(extension.id,
                                                  callback(function(warnings) {
        chrome.test.assertEq(1, warnings.length);
        chrome.test.assertEq("Manage your apps, extensions, and themes",
                             warnings[0]);
      }));
    }));
  },

  function permissionWarningsClipboardReadApi() {
    var manifest_str =
        `{
           "name": "Clipboard!",
           "version": "1.0",
           "manifest_version": 2,
           "permissions": ["clipboardRead"]
         }`;

    chrome.management.getPermissionWarningsByManifest(
        manifest_str, callback(function(warnings) {
      chrome.test.assertEq(1, warnings.length);
      chrome.test.assertEq("Read data you copy and paste", warnings[0]);
    }));
  },

  // Disables an enabled app.
  function disable() {
    listenOnce(chrome.management.onDisabled, function(info) {
      assertEq(info.name, "enabled_app");
    });

    chrome.management.getAll(callback(function(items) {
      var enabled_app = getItemNamed(items, "enabled_app");
      checkItem(enabled_app, "enabled_app", true, "hosted_app");
      chrome.management.setEnabled(enabled_app.id, false, callback(function() {
        chrome.management.get(enabled_app.id, callback(function(now_disabled) {
          checkItem(now_disabled, "enabled_app", false, "hosted_app");
        }));
      }));
    }));
  },

  // Enables a disabled extension.
  function enable() {
    listenOnce(chrome.management.onEnabled, function(info) {
      assertEq(info.name, "disabled_extension");
    });
    chrome.management.getAll(callback(function(items) {
      var disabled = getItemNamed(items, "disabled_extension");
      checkItem(disabled, "disabled_extension", false, "extension");
      chrome.management.setEnabled(disabled.id, true, callback(function() {
        chrome.management.get(disabled.id, callback(function(now_enabled) {
          checkItem(now_enabled, "disabled_extension", true, "extension");
        }));
      }));
    }));
  }
];

const scriptUrl = '_test_resources/api_test/management/common.js';
let loadScript = chrome.test.loadScript(scriptUrl);

loadScript.then(async function() {
  chrome.test.runTests(tests);
});
