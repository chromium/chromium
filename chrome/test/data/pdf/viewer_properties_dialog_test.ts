// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

async function ensurePropertiesDialogOpen() {
  assertNull(getPropertiesDialog());
  const whenOpen = eventToPromise('cr-dialog-open', viewer);
  const toolbar = viewer.shadowRoot.querySelector('viewer-toolbar')!;
  toolbar.dispatchEvent(new CustomEvent('properties-click'));
  await whenOpen;
  assertTrue(!!getPropertiesDialog());
}

async function ensurePropertiesDialogClose() {
  const dialog = getPropertiesDialog();
  assertTrue(!!dialog);
  const whenClosed = eventToPromise('close', dialog);
  dialog.$.close.click();
  await whenClosed;
  assertNull(getPropertiesDialog());
}

function getPropertiesDialog() {
  return viewer.shadowRoot.querySelector('viewer-properties-dialog');
}

function assertField(field: string, expectedValue: string) {
  const dialog = getPropertiesDialog();
  assertTrue(!!dialog);
  const actualValue = dialog.shadowRoot.querySelector<HTMLElement>(
                                           `#${field}`)!.textContent.trim();
  assertEquals(expectedValue, actualValue);
}

const EXPECTED_PROPERTIES: Array<[string, string]> = [
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
];

suite('ViewerPropertiesDialog', function() {
  suiteSetup(async function() {
    await ensurePropertiesDialogOpen();
  });

  suiteTeardown(async function() {
    await ensurePropertiesDialogClose();
  });

  for (const [field, expectedValue] of EXPECTED_PROPERTIES) {
    test(field, function() {
      assertField(field, expectedValue);
    });
  }
});
