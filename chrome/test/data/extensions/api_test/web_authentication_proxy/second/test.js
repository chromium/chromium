// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a second extension that WebAuthenticationProxyApiTest loads to test
// behavior with multiple installed proxy extensions. Test names correspond to
// ones in the parent directory's test.js file.

const ERROR_ATTACH = 'Error: Another extension is already attached';

let availableTests = [
  async function attachSecondExtension() {
    await chrome.test.assertPromiseRejects(
        chrome.webAuthenticationProxy.attach(), ERROR_ATTACH);
    await chrome.test.sendMessage('attachFailed');
    await chrome.webAuthenticationProxy.attach();
    chrome.test.succeed();
  },
];

chrome.test.getConfig((config) => {
  const tests = availableTests.filter((t) => {
    return config.customArg == t.name;
  });
  if (tests.length == 0) {
    chrome.test.notifyFail('No test found');
    return;
  }
  chrome.test.runTests(tests);
});
