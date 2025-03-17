// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var inDeveloperMode = config.customArg == 'in_developer_mode';

  chrome.test.runTests([async function testDevModeApiAccess() {
    if (inDeveloperMode) {
      // In developer mode, the API should be defined...
      chrome.test.assertTrue(!!chrome.debugger);
      chrome.test.assertTrue(!!chrome.debugger.getTargets);

      // ... And, just as importantly, should be usable. (We don't need to
      // check the whole implementation here -- just verifying the API calls
      // don't throw is sufficient.)
      const tabs = await chrome.tabs.query({});
      chrome.test.assertEq(1, tabs.length);
      const debuggee = {tabId: tabs[0].id};
      await chrome.debugger.attach(debuggee, '1.3');
      await chrome.debugger.detach(debuggee);
      chrome.test.succeed();
    } else {
      const expectedError =
          `Failed to read the 'debugger' property from 'Object': The ` +
          `'debugger' API is only available for users in developer mode.`;
      var functionThatThrows = function() {
        chrome.debugger;
      };
      chrome.test.assertThrows(functionThatThrows, [], expectedError);
    }
    chrome.test.succeed();
  }]);
});
