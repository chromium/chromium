// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Content settings API test
// Run with browser_tests
//     --gtest_filter=ExtensionApiTest.ContentSettingsGetResourceIdentifiers

Object.prototype.forEach = function(f) {
  for (key in this) {
    if (this.hasOwnProperty(key))
      f(key, this[key]);
  }
}

var cs = chrome.contentSettings;
chrome.test.runTests([
  function getResourceIdentifiers() {
    var contentTypes = {
      "cookies": undefined,
      "images": undefined,
      "javascript": undefined,
      "plugins": [
        {
          "description": "Foo",
          "id": "foo",
        },
        {
          "description": "Bar Plugin",
          "id": "bar.plugin",
        },
      ],
      "popups": undefined,
      "notifications": undefined
    };
    contentTypes.forEach(function(type, identifiers) {
      cs[type].getResourceIdentifiers(chrome.test.callbackPass(function(value) {
        chrome.test.assertEq(identifiers, value);
      }));
    });
  },
]);
