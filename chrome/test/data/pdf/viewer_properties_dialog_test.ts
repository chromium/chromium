// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

async function ensurePropertiesDialogOpen() {
  chrome.test.assertFalse(!!getPropertiesDialog());
  const whenOpen = eventToPromise('cr-dialog-open', viewer);
  const toolbar = viewer.shadowRoot!.querySelector('viewer-toolbar')!;
  toolbar.dispatchEvent(new CustomEvent('properties-click'));
  await whenOpen;
  chrome.test.assertTrue(!!getPropertiesDialog());
}

async function ensurePropertiesDialogClose() {
  const dialog = getPropertiesDialog();
  chrome.test.assertTrue(!!dialog);
  const whenClosed = eventToPromise('close', dialog);
  dialog.$.close.click();
  await whenClosed;
  chrome.test.assertFalse(!!getPropertiesDialog());
}

function getPropertiesDialog() {
  return viewer.shadowRoot!.querySelector('viewer-properties-dialog')!;
}

function assertField(field: string, expectedValue: string) {
  const actualValue =
      getPropertiesDialog().shadowRoot!.querySelector<HTMLElement>(
                                           `#${field}`)!.textContent!.trim();
  chrome.test.assertEq(expectedValue, actualValue);
}

const tests = [
  async function testPropertiesDialog() {
    await ensurePropertiesDialogOpen();

    ([
      ['file-name', 'document_info.pdf'],
      ['file-size', '714 B'],
      ['title', 'Sample PDF Document Info'],
      ['author', 'Chromium Authors'],
      ['subject', 'Testing'],
      ['keywords', 'testing,chromium,pdfium,document,info'],
      ['created', '2/5/20, 7:39:12\u202fAM'],
      ['modified', '2/6/20, 1:42:34\u202fAM'],
      ['application', 'Your Preferred Text Editor'],
      ['pdf-producer', 'fixup_pdf_template.py'],
      ['pdf-version', '1.7'],
      ['page-count', '1'],
      ['page-size', '2.78 × 2.78 in (square)'],
      ['fast-web-view', 'No'],
    ] as Array<[string, string]>)
        .forEach(([field, expectedValue]) => assertField(field, expectedValue));

    await ensurePropertiesDialogClose();

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
