// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testGetStrings() {
    chrome.resourcesPrivate.getStrings(
        "identity", chrome.test.callbackPass(function(data) {
          // String from the identity component.
          chrome.test.assertTrue('window-title' in data);
          // String from the loadTimeData defaults.
          chrome.test.assertTrue('language' in data);
          chrome.test.succeed();
        }));
  }
]);
