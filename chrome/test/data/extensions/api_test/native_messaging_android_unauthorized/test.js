// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function testUnauthorizedExtension() {
    const APP_NAME = 'org.chromium.chrome.tests.support';
    const expectedError =
        'Error: Access to native messaging is unauthorized for this extension.';

    await chrome.test.assertPromiseRejects(
        chrome.runtime.sendNativeMessage(APP_NAME, {text: 'hello'}),
        expectedError);
    chrome.test.succeed();
  },
]);
