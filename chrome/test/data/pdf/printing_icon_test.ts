// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testPrintingEnabled() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.shadowRoot!.querySelector('viewer-toolbar')!;
    toolbar.printingEnabled = true;
    const printIcon = toolbar.shadowRoot!.querySelector<HTMLElement>('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertFalse(printIcon!.hidden);
    chrome.test.succeed();
  },
  function testPrintingDisabled() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.shadowRoot!.querySelector('viewer-toolbar')!;
    toolbar.printingEnabled = false;
    const printIcon = toolbar.shadowRoot!.querySelector<HTMLElement>('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertTrue(printIcon!.hidden);
    chrome.test.succeed();
  },
]);
