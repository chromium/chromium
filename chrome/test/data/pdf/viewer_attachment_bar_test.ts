// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerAttachmentBarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer_attachment_bar.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createAttachmentBar(): ViewerAttachmentBarElement {
  document.body.innerHTML = '';
  const attachmentBar = document.createElement('viewer-attachment-bar');
  document.body.appendChild(attachmentBar);
  return attachmentBar;
}

// Unit tests for the viewer-attachment-bar element.
const tests = [
  async function testWithRegularAttachment() {
    const attachmentBar = createAttachmentBar();
    attachmentBar.attachments = [
      {name: 'attachment1', size: 10, readable: true},
      {name: 'attachment2', size: 1, readable: true},
    ];
    await microtasksFinished();

    // No warning message is displayed.
    const warning = attachmentBar.shadowRoot!.querySelector('#warning')!;
    chrome.test.assertFalse(warning.getAttribute('hidden') === null);
    chrome.test.succeed();
  },

  async function testWithOversizeAttachment() {
    const attachmentBar = createAttachmentBar();
    attachmentBar.attachments = [
      {name: 'attachment1', size: 10, readable: true},
      {name: 'attachment2', size: -1, readable: true},
    ];
    await microtasksFinished();

    // A warning message is displayed because `attachment2` is oversized.
    const warning = attachmentBar.shadowRoot!.querySelector('#warning')!;
    chrome.test.assertEq(null, warning.getAttribute('hidden'));
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
