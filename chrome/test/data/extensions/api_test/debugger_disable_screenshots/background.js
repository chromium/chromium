// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getSingleTab} from '/_test_resources/test_util/tabs_util.js';

const protocolVersion = '1.3';

chrome.test.runTests([
  async function testAttachRejectedWhenScreenshotsDisabled() {
    const config = await chrome.test.getConfig();
    const expectedError = config.customArg;
    // Attach to the existing tab navigated by the C++ test rather than opening
    // a new one, ensuring the WebContents instance matches test expectations.
    const tab = await getSingleTab({active: true});
    const debuggee = {tabId: tab.id};

    // Attempting to attach must fail outright when screenshots are restricted
    // by enterprise policy.
    chrome.debugger.attach(
        debuggee, protocolVersion,
        chrome.test.callbackFail(expectedError, () => {
          chrome.test.succeed();
        }));
  },
]);

