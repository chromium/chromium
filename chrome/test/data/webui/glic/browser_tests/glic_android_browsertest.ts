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
    const result = await this.host.getContextFromFocusedTab?.({
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
  }

  async testDeviceRotationMojoResiliency() {
    assertDefined(this.host);
  }
}

testMain([
  GlicAndroidBrowserTests,
]);
