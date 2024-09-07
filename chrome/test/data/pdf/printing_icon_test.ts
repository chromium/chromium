// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {microtasksFinished} from 'chrome://webui-test/test_util.js';

chrome.test.runTests([
  async function testPrintingEnabled() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.shadowRoot!.querySelector('viewer-toolbar')!;
    toolbar.printingEnabled = true;
    await microtasksFinished();
    const printIcon = toolbar.shadowRoot!.querySelector<HTMLElement>('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertFalse(printIcon!.hidden);
    chrome.test.succeed();
  },
  async function testPrintingDisabled() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.shadowRoot!.querySelector('viewer-toolbar')!;
    toolbar.printingEnabled = false;
    await microtasksFinished();
    const printIcon = toolbar.shadowRoot!.querySelector<HTMLElement>('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertTrue(printIcon!.hidden);
    chrome.test.succeed();
  },
]);
