// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function(launchData) {
  // Tests that the received launch data contains the action data object that
  // matches the test expectation.
  chrome.test.runTests([
    function compareActionData() {
      chrome.test.assertTrue(!!launchData.actionData);

      chrome.test.sendMessage(
          'getExpectedActionData',
          chrome.test.callbackPass(function(actionData) {
            chrome.test.assertEq(JSON.parse(actionData), launchData.actionData);
          }));
    }
  ]);
});
