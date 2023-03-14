// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  /** "allowed-domain.com" is a domain on the test allowlist. */
  async function testAllowedDomain() {
    const result = await chrome.pdfViewerPrivate.isAllowedLocalFileAccess(
        'https://www.allowed-domain.com/document.pdf');
    chrome.test.assertTrue(result);
    chrome.test.succeed();
  },
  /** "disallowed-domain.com" is not on the test allowlist. */
  async function testDisallowedDomain() {
    const result = await chrome.pdfViewerPrivate.isAllowedLocalFileAccess(
        'https://www.disallowed-domain.com/document.pdf');
    chrome.test.assertFalse(result);
    chrome.test.succeed();
  },
  /**
   * Test that only https scheme is allowed local file access, even with an
   * allowed domain.
   */
  async function testDisallowedSchemes() {
    const nonHttpsUrls = [
      'http://www.allowed-domain.com/document.pdf',
      'file://www.allowed-domain.com/document.pdf',
      'data:,www.allowed-domain.com%2Fdocument.pdf',
      'chrome://www.allowed-domain.com/document.pdf',
      'javascript://www.allowed-domain.com/document.pdf',
      'invalid-scheme://www.allowed-domain.com/document.pdf'
    ];
    for (const url in nonHttpsUrls) {
      const result =
          await chrome.pdfViewerPrivate.isAllowedLocalFileAccess(url);
      chrome.test.assertFalse(result, 'Unexpected result for: ' + url);
    }
    chrome.test.succeed();
  },
  /**
   * Test that invalid values in the policy are ignored.
   */
  async function testInvalidPolicy() {
    const result = await chrome.pdfViewerPrivate.isAllowedLocalFileAccess('10');
    chrome.test.assertFalse(result);
    chrome.test.succeed();
  },
  /**
   * Test that the isPdfOcrAlwaysActive function can fetch the pref correctly.
   * This function fetches the pref defined with the following name:
   * `settings.a11y.pdf_ocr_always_active`
   */
  async function testIsPdfOcrAlwaysActive() {
    const isPdfOcrAlwaysActive =
        await chrome.pdfViewerPrivate.isPdfOcrAlwaysActive();
    chrome.test.assertNe(null, isPdfOcrAlwaysActive);
    chrome.test.succeed();
  },
  /**
   * Test that the setPdfOcrPref function can set the pref correctly.
   * This function sets the pref defined with the following name:
   * `settings.a11y.pdf_ocr_always_active`
   */
  async function testSetPdfOcrPrefToTrue() {
    const result = await chrome.pdfViewerPrivate.setPdfOcrPref(true);
    chrome.test.assertTrue(result);
    const isPdfOcrAlwaysActive =
        await chrome.pdfViewerPrivate.isPdfOcrAlwaysActive();
    chrome.test.assertTrue(isPdfOcrAlwaysActive);
    chrome.test.succeed();
  },
  async function testSetPdfOcrPrefToFalse() {
    const result = await chrome.pdfViewerPrivate.setPdfOcrPref(false);
    chrome.test.assertTrue(result);
    const isPdfOcrAlwaysActive =
        await chrome.pdfViewerPrivate.isPdfOcrAlwaysActive();
    chrome.test.assertFalse(isPdfOcrAlwaysActive);
    chrome.test.succeed();
  },
  /**
   * Test that the onPdfOcrPrefChanged function can monitor changes in the PDF
   * OCR pref correctly. It monitors the pref defined with the following name:
   * `settings.a11y.pdf_ocr_always_active`
   */
  async function testOnPdfOcrPrefChanged() {
    const result = await chrome.pdfViewerPrivate.setPdfOcrPref(false);
    chrome.test.assertTrue(result);

    chrome.pdfViewerPrivate.onPdfOcrPrefChanged.addListener(function local(
        isPdfOcrAlwaysActive) {
      chrome.pdfViewerPrivate.onPdfOcrPrefChanged.removeListener(local);
      chrome.test.assertTrue(isPdfOcrAlwaysActive);
      chrome.test.succeed();
    });

    chrome.pdfViewerPrivate.setPdfOcrPref(true);
  }
]);
