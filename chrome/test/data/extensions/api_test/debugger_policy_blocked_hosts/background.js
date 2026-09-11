// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getSingleTab} from '/_test_resources/test_util/tabs_util.js';

const protocolVersion = '1.3';

chrome.test.runTests([
  async function testAttach() {
    const config = await chrome.test.getConfig();
    const expectedError = config.customArg;
    const tab = await getSingleTab({active: true});
    const debuggee = {tabId: tab.id};

    if (expectedError) {
      await chrome.test.assertPromiseRejects(
          chrome.debugger.attach(debuggee, protocolVersion),
          new RegExp(expectedError));
    } else {
      await chrome.debugger.attach(debuggee, protocolVersion);
      await chrome.debugger.detach(debuggee);
    }
    chrome.test.succeed();
  },
]);

