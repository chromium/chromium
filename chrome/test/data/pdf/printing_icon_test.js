// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

chrome.test.runTests([
  function testPrintingEnabled() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('#viewer'));
    const toolbar = viewer.shadowRoot.querySelector('#toolbar');
    toolbar.printingEnabled = true;
    const printIcon = toolbar.shadowRoot.querySelector('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertFalse(printIcon.hidden);
    chrome.test.succeed();
  },
  function testPrintingDisabled() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('#viewer'));
    const toolbar = viewer.shadowRoot.querySelector('#toolbar');
    toolbar.printingEnabled = false;
    const printIcon = toolbar.shadowRoot.querySelector('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertTrue(printIcon.hidden);
    chrome.test.succeed();
  },
]);
