// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {ViewerPropertiesDialogElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/elements/viewer-properties-dialog.js';
import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer.js';

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

    // TODO(crbug.com/93169): None of the following expected values should be
    // '-' when support for every property is implemented.
    [['file-name', '-'],
     ['file-size', '-'],
     ['title', 'Sample PDF Document Info'],
     ['author', 'Chromium Authors'],
     ['subject', 'Testing'],
     ['keywords', '-'],
     ['created', '-'],
     ['modified', '-'],
     ['application', 'Your Preferred Text Editor'],
     ['pdf-producer', 'fixup_pdf_template.py'],
     ['pdf-version', '-'],
     ['page-count', '-'],
     ['page-size', '-'],
     ['fast-web-view', '-'],
    ].forEach(([field, expectedValue]) => assertField(field, expectedValue));

    await ensurePropertiesDialogClose();

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
