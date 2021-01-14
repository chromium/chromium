// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tests = [
  function simple() {
    chrome.developerPrivate.getItemsInfo(true, // include disabled
                                         true, // include terminated
                                         callback(function(items) {
      chrome.test.assertEq(3, items.length);

      checkItemInList(items, "hosted_app", true, "hosted_app",
          { "app_launch_url": "http://www.google.com/",
            "offline_enabled": true,
            "update_url": "http://example.com/update.xml" });

      checkItemInList(items, "simple_extension", true, "extension",
          { "homepage_url": "http://example.com/",
            "options_url": "chrome-extension://<ID>/pages/options.html"});

      var extension = getItemNamed(items, "packaged_app");
      checkItemInList(items, "packaged_app", true, "packaged_app",
          { "offline_enabled": true});
    }));
  },
  function aliasedFunctions() {
    // The allow file access and allow incognito functions are aliased with
    // custom bindings. Test that they work as expected.
    var getExtensionInfoCallback = chrome.test.callbackAdded();
    chrome.developerPrivate.getExtensionsInfo(function(infos) {
      var info = null;
      for (var i = 0; i < infos.length; ++i) {
        if (infos[i].name == 'simple_extension') {
          info = infos[i];
          break;
        }
      }
      chrome.test.assertTrue(info != null);
      var extId = info.id;
      chrome.test.assertFalse(info.incognitoAccess.isActive);
      chrome.test.assertTrue(info.fileAccess.isActive);
      chrome.test.assertEq(chrome.developerPrivate.ExtensionState.ENABLED,
                           info.state);
      var allowIncognitoCallback = chrome.test.callbackAdded();
      chrome.test.runWithUserGesture(function() {
        chrome.developerPrivate.allowIncognito(extId, true, function() {
          chrome.developerPrivate.getExtensionInfo(extId, function(info) {
            chrome.test.assertTrue(info.incognitoAccess.isActive);
            allowIncognitoCallback();
          });
        });
      });
      var allowFileAccessCallback = chrome.test.callbackAdded();
      chrome.test.runWithUserGesture(function() {
        chrome.developerPrivate.allowFileAccess(extId, false, function() {
          chrome.developerPrivate.getExtensionInfo(extId, function(info) {
            chrome.test.assertFalse(info.fileAccess.isActive);
            allowFileAccessCallback();
          });
        });
      });

      getExtensionInfoCallback();
    });
  }
];

chrome.test.runTests(tests);
