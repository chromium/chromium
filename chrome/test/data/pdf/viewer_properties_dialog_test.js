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

const tests = [
  async function testPropertiesDialog() {
    await ensurePropertiesDialogOpen();
    await ensurePropertiesDialogClose();
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
