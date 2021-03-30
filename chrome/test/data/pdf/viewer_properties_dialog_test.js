// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {PDFViewerElement, ViewerPropertiesDialogElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const viewer = /** @type {!PDFViewerElement} */ (
    document.body.querySelector('pdf-viewer'));

async function ensurePropertiesDialogOpen() {
  chrome.test.assertFalse(!!getPropertiesDialog());
  const whenOpen = eventToPromise('cr-dialog-open', viewer);
  const toolbar = viewer.shadowRoot.querySelector('viewer-pdf-toolbar-new');
  toolbar.dispatchEvent(new CustomEvent('properties-click'));
  await whenOpen;
  chrome.test.assertTrue(!!getPropertiesDialog());
}

async function ensurePropertiesDialogClose() {
  const dialog = getPropertiesDialog();
  chrome.test.assertTrue(!!dialog);
  const whenClosed = eventToPromise('close', dialog);
  dialog.shadowRoot.querySelector('#close').click();
  await whenClosed;
  chrome.test.assertFalse(!!getPropertiesDialog());
}

/** @return {!ViewerPropertiesDialogElement} */
function getPropertiesDialog() {
  return /** @type {!ViewerPropertiesDialogElement} */ (
      viewer.shadowRoot.querySelector('#properties-dialog'));
}

/**
 * @param {string} field
 * @param {string} expectedValue
 */
function assertField(field, expectedValue) {
  const actualValue = getPropertiesDialog()
                          .shadowRoot.querySelector(`#${field}`)
                          .textContent.trim();
  chrome.test.assertEq(expectedValue, actualValue);
}

const tests = [
  async function testPropertiesDialog() {
    await ensurePropertiesDialogOpen();

    [['file-name', 'document_info.pdf'],
     ['file-size', '714 B'],
     ['title', 'Sample PDF Document Info'],
     ['author', 'Chromium Authors'],
     ['subject', 'Testing'],
     ['keywords', 'testing,chromium,pdfium,document,info'],
     ['created', '2/5/20, 7:39:12 AM'],
     ['modified', '2/6/20, 1:42:34 AM'],
     ['application', 'Your Preferred Text Editor'],
     ['pdf-producer', 'fixup_pdf_template.py'],
     ['pdf-version', '1.7'],
     ['page-count', '1'],
     ['page-size', '2.78 × 2.78 in (portrait)'],
     ['fast-web-view', 'No'],
    ].forEach(([field, expectedValue]) => assertField(field, expectedValue));

    await ensurePropertiesDialogClose();

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
