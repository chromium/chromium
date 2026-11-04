// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// cc_file_path: chrome/browser/glic/android/glic_android_browsertest.cc

import {ApiTestFixtureBase, assertDefined, assertEquals, assertFalse, assertTrue, readStream, testMain} from './browser_test_base.js';

class GlicAndroidBrowserTests extends ApiTestFixtureBase {
  override async setUpTest() {
    await this.client.waitForFirstOpen();
  }

  async testPageContextFetching() {
    const result = await this.host.getContextFromFocusedTab?.({
      viewportScreenshot: false,
    });

    assertDefined(result);
    assertTrue(
        result.tabData.url.endsWith('/page.html'),
        `Tab data has unexpected url ${result.tabData.url}`);
  }

  async testPageContextFetchingWithPdf() {
    await this.checkPdfPageContext();
  }

  async testPageContextFetchingWithWebPdf() {
    const result = await this.checkPdfPageContext();
    assertEquals('https://example.com:443', result.pdfDocumentData?.origin);
  }

  // Fetches the context of the focused PDF tab and verifies it, then returns
  // the fetched context.
  private async checkPdfPageContext() {
    const result = await this.host.getContextFromFocusedTab?.({
      innerText: true,
      pdfData: true,
      viewportScreenshot: false,
    });

    assertDefined(result);
    assertEquals('application/pdf', result.tabData.documentMimeType);
    assertDefined(result.pdfDocumentData);
    assertDefined(result.pdfDocumentData.pdfData);
    const pdfData: Uint8Array =
        await readStream(result.pdfDocumentData.pdfData);
    // The fetched PDF bytes must match the content of the PDF file written by
    // the C++ side of the test.
    const expectedPdfContent = this.testParams as string;
    assertEquals(expectedPdfContent, new TextDecoder().decode(pdfData));
    assertFalse(result.pdfDocumentData.pdfSizeLimitExceeded);
    // The title must be the one of the native PDF page set by the C++ side of
    // the test, rather than the one of the placeholder document.
    assertEquals(
        'test.pdf', result.tabData.title,
        `Tab data has unexpected title ${result.tabData.title}`);

    const details = `url=${result.tabData.url}, mainDocument.origin=${
        result.webPageData?.mainDocument.origin}, pdf origin=${
        result.pdfDocumentData.origin}`;
    // The main document must have the same origin as the PDF, like on desktop.
    assertDefined(result.webPageData);
    assertEquals(
        result.pdfDocumentData.origin, result.webPageData.mainDocument.origin,
        details);
    return result;
  }

  async testDeviceRotationMojoResiliency() {
    assertDefined(this.host);
  }
}

testMain([
  GlicAndroidBrowserTests,
]);
