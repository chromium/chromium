// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let documentId;
let documentUrl;

chrome.test.runTests([
  // Ensure we find the test tab of the incognito window and load it
  // documentId.
  async function setup() {
    const config = await chrome.test.getConfig();
    documentUrl = `http://a.com:${config.testServer.port}/empty.html`;
    const tabs = await chrome.tabs.query({url: documentUrl});
    chrome.test.assertEq(tabs.length, 1);
    chrome.test.assertTrue(tabs[0].incognito);
    const frame =
        await chrome.webNavigation.getFrame({tabId: tabs[0].id, frameId: 0});
    documentId = frame.documentId;
    chrome.test.succeed();
  },

  // Verify getFrame via documentId works correctly in incognito mode.
  async function testGetFrame() {
    const details =
        await chrome.webNavigation.getFrame({documentId: documentId});

    chrome.test.assertEq(
        {
          errorOccurred: false,
          url: documentUrl,
          parentFrameId: -1,
          documentId: documentId,
          documentLifecycle: 'active',
          frameType: 'outermost_frame',
        },
        details);
    chrome.test.succeed();
  },
]);
