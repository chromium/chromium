// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getSingleTab} from '/_test_resources/test_util/tabs_util.js';

const protocolVersion = '1.3';

chrome.test.runTests([
  async function testAttach() {
    const config = await chrome.test.getConfig();
    const debuggerAllowed = config.customArg === 'debuggerAllowed';
    // Attach to the existing tab navigated by the C++ test rather than opening
    // a new one, ensuring the WebContents instance matches test expectations.
    const tab = await getSingleTab({active: true});
    const debuggee = {tabId: tab.id};

    if (debuggerAllowed) {
      await chrome.debugger.attach(debuggee, protocolVersion);
      await chrome.debugger.detach(debuggee);
      chrome.test.succeed();
    } else {
      const expectedError = config.customArg;
      chrome.test.assertTrue(
          Boolean(expectedError), 'expectedError must be specified');
      // Attempting to attach must fail outright when screenshots are restricted
      // by enterprise policy and strict policy checks are enabled.
      chrome.debugger.attach(
          debuggee, protocolVersion,
          chrome.test.callbackFail(expectedError, () => {
            chrome.test.succeed();
          }));
    }
  },
]);

