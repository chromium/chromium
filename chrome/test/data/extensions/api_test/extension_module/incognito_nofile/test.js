// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// extension api test
// browser_tests.exe --gtest_filter=ExtensionModuleApiTest.IncognitoNofile

chrome.test.runTests([
  function testPermissions() {
    chrome.extension.isAllowedIncognitoAccess(
        chrome.test.callbackPass(function(hasAccess) {
          chrome.test.assertTrue(hasAccess);
        }));
    chrome.extension.isAllowedFileSchemeAccess(
        chrome.test.callbackPass(function(hasAccess) {
          chrome.test.assertFalse(hasAccess);
        }));
  }
]);
