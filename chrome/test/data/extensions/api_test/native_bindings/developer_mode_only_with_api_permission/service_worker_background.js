// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var inDeveloperMode = config.customArg == 'in_developer_mode';

  chrome.test.runTests([
    function testDebuggerApiAccess() {
      if (inDeveloperMode) {
        chrome.test.assertTrue(!!chrome.debugger);
        chrome.test.assertTrue(!!chrome.debugger.getTargets);
      } else {
        var expectedError = 'The \'debugger\' API is only '
                          + 'available for users in developer mode.';
        var functionThatThrows = function() {
          chrome.debugger;
        };
        chrome.test.assertThrows(functionThatThrows, [], expectedError);
      }
      chrome.test.succeed();
    }
  ]);
});
