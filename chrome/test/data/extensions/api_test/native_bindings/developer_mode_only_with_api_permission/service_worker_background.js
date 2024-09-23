// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.getConfig(function(config) {
  var inDeveloperMode = config.customArg == 'in_developer_mode';

  chrome.test.runTests([
    async function testDebuggerApiAccess() {
      if (inDeveloperMode) {
        // In developer mode, the API should be defined...
        chrome.test.assertTrue(!!chrome.userScripts);
        chrome.test.assertTrue(!!chrome.userScripts.register);
        chrome.test.assertTrue(!!chrome.userScripts.getScripts);

        // ... And, just as importantly, should be usable. (We don't need to
        // check the whole implementation here -- just verifying the API calls
        // don't throw is sufficient.)
        const script =
            {
              id: 'script',
              matches: ['*://*/*'],
              js: [{file: 'script.js'}]
            };
        await chrome.userScripts.register([script]);
        const registered = await chrome.userScripts.getScripts();
        chrome.test.assertEq(1, registered.length);
        chrome.test.assertEq('script', registered[0].id);
      } else {
        const expectedError =
            `Failed to read the 'userScripts' property from 'Object': The ` +
            `'userScripts' API is only available for users ` +
            'in developer mode.';
        // Trying to access the API should throw an error outside of developer
        // mode.
        const functionThatThrows = function() {
          chrome.userScripts;
        };
        chrome.test.assertThrows(functionThatThrows, [], expectedError);
      }
      chrome.test.succeed();
    }
  ]);
});
