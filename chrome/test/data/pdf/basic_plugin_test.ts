// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PdfScriptingApi} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';
import {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

/**
 * These tests require that the PDF plugin be available to run correctly.
 */
chrome.test.runTests([
  /**
   * Test that the page is sized to the size of the document.
   */
  function testPageSize() {
    const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
    // Verify that the initial zoom is less than or equal to 100%.
    chrome.test.assertTrue(viewer.viewport.getZoom() <= 1);

    viewer.viewport.setZoom(1);
    chrome.test.assertEq(826, viewer.viewport.contentSize.width);
    chrome.test.assertEq(1066, viewer.viewport.contentSize.height);
    chrome.test.succeed();
  },

  function testGetSelectedText() {
    const client = new PdfScriptingApi(window, window);
    client.selectAll();
    client.getSelectedText(function(selectedText) {
      chrome.test.assertEq('this is some text\nsome more text', selectedText);
      chrome.test.succeed();
    });
  },

  /**
   * Test that the filename is used as the title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertEq('test.pdf', document.title);
    chrome.test.succeed();
  },
]);
