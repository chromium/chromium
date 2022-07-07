// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  async function createDocumentAndEnsureItExists() {
    await chrome.offscreen.createDocument(
        {
          url: 'offscreen.html',
          reasons: ['TESTING'],
          justification: 'ignored'
        });
    // Sanity check that the document exists and can be reached by passing a
    // message and expecting a reply. Note that general offscreen document
    // behavior is tested more in the OffscreenDocumentHost tests, so this is
    // mostly just ensuring that it works when the document is created from the
    // API.
    const reply = await chrome.runtime.sendMessage('message from background');
    chrome.test.assertEq(
        {
          msg: 'message from background',
          reply: 'offscreen reply',
        },
        reply);
    chrome.test.succeed();
  },
])
