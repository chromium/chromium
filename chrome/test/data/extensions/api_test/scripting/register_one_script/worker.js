// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function registerOneScript() {
    const scriptsToRegister = [
      {
        id: 'GRS_1',
        matches: ['*://*/*'],
        js: ['/change_title.js']
      }
    ];

    let scripts = await chrome.scripting.getRegisteredContentScripts({});
    // This extension registers `scriptsToRegister` if and only if the script
    // has not been registered before and trying to re-register an existing
    // script throws an error, hence this check.
    if (scripts.length === 0)
      await chrome.scripting.registerContentScripts(scriptsToRegister);

    scripts = await chrome.scripting.getRegisteredContentScripts({});
    chrome.test.assertEq(1, scripts.length);
    chrome.test.assertEq('GRS_1', scripts[0].id);
    chrome.test.succeed();
  },
]);
