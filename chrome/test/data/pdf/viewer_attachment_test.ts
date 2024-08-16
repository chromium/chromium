// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerAttachmentElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer_attachment.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

function createAttachment(): ViewerAttachmentElement {
  document.body.innerHTML = '';
  const attachment = document.createElement('viewer-attachment');
  document.body.appendChild(attachment);
  return attachment;
}

// Unit tests for the viewer-attachment element.
const tests = [
  async function testWithRegularAttachment() {
    const viewerAttachment = createAttachment();
    viewerAttachment
        .attachment = {name: 'attachment1', size: 10, readable: true};
    viewerAttachment.index = 0;
    await microtasksFinished();

    const downloadButton =
        viewerAttachment.shadowRoot!.querySelector('cr-icon-button')!;
    const attachmentTitle =
        viewerAttachment.shadowRoot!.querySelector('#title')!;
    chrome.test.assertFalse(downloadButton.hidden);
    chrome.test.assertEq('1', window.getComputedStyle(attachmentTitle).opacity);
    chrome.test.succeed();
  },

  async function testWithOversizedAttachment() {
    const viewerAttachment = createAttachment();
    viewerAttachment
        .attachment = {name: 'attachment1', size: -1, readable: true};
    viewerAttachment.index = 0;
    await microtasksFinished();

    const downloadButton =
        viewerAttachment.shadowRoot!.querySelector('cr-icon-button')!;
    const attachmentTitle =
        viewerAttachment.shadowRoot!.querySelector('#title')!;

    // An oversized attachment will not have a download button, and its title
    // will be grayed out.
    chrome.test.assertTrue(downloadButton.hidden);
    chrome.test.assertEq(
        '0.38', window.getComputedStyle(attachmentTitle).opacity);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
