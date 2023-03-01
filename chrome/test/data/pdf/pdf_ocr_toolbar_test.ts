// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

function createToolbar() {
  document.body.innerHTML = '';
  const toolbar = document.createElement('viewer-toolbar');
  document.body.appendChild(toolbar);
  return toolbar;
}

function assertMenuButtonChecked(button: HTMLElement) {
  chrome.test.assertEq('true', button.getAttribute('aria-checked'));
  chrome.test.assertFalse(button.querySelector('iron-icon')!.hidden);
}

function assertMenuButtonUnchecked(button: HTMLElement) {
  chrome.test.assertEq('false', button.getAttribute('aria-checked'));
  chrome.test.assertTrue(button.querySelector('iron-icon')!.hidden);
}

// Unit tests for the PDF OCR toggle on the viewer-toolbar element.
const tests = [
  async function testPdfOcrToggle() {
    const toolbar = createToolbar();
    toolbar.pdfOcrEnabled = true;
    await waitAfterNextRender(toolbar);

    const button =
        toolbar.shadowRoot!.querySelector<HTMLElement>('#pdf-ocr-button')!;
    chrome.test.assertTrue(!!button);
    assertMenuButtonUnchecked(button);

    // Enabling PDF OCR makes the button checked.
    button.click();
    // Block so that PdfViewerPrivateProxy.setPdfOcrPref can run.
    await waitAfterNextRender(button);
    // Wait one more time to ensure that the toggle button has been updated via
    // PdfViewerPrivateProxy.onPdfOcrPrefChanged.
    await waitAfterNextRender(button);
    assertMenuButtonChecked(button);

    // Disabling PDF OCR makes the button unchecked.
    button.click();
    // Block so that PdfViewerPrivateProxy.setPdfOcrPref can run.
    await waitAfterNextRender(button);
    // Wait one more time to ensure that the toggle button has been updated via
    // PdfViewerPrivateProxy.onPdfOcrPrefChanged.
    await waitAfterNextRender(button);
    assertMenuButtonUnchecked(button);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
