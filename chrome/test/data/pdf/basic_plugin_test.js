// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';

/**
 * These tests require that the PDF plugin be available to run correctly.
 */
const tests = [
  /**
   * Test that the page is sized to the size of the document.
   */
  function testPageSize() {
    // Verify that the initial zoom is less than or equal to 100%.
    chrome.test.assertTrue(viewer.viewport.getZoom() <= 1);

    viewer.viewport.setZoom(1);
    const sizer = document.getElementById('sizer');
    chrome.test.assertEq(826, sizer.offsetWidth);
    chrome.test.assertEq(
        1066 + viewer.viewport.topToolbarHeight_, sizer.offsetHeight);
    chrome.test.succeed();
  },

  function testGetSelectedText() {
    const client = new PDFScriptingAPI(window, window);
    client.selectAll();
    client.getSelectedText(chrome.test.callbackPass(function(selectedText) {
      chrome.test.assertEq('this is some text\nsome more text', selectedText);
    }));
  },

  /**
   * Test that the filename is used as the title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertEq('test.pdf', document.title);

    chrome.test.succeed();
  },

  /**
   * Test that the escape key gets propogated to the outer frame (via the
   * PDFScriptingAPI) in print preview.
   */
  function testEscKeyPropogationInPrintPreview() {
    viewer.isPrintPreview_ = true;
    scriptingAPI.setKeyEventCallback(chrome.test.callbackPass(function(e) {
      chrome.test.assertEq(27, e.keyCode);
      chrome.test.assertEq('Escape', e.code);
    }));
    const e = document.createEvent('Event');
    e.initEvent('keydown');
    e.keyCode = 27;
    e.code = 'Escape';
    document.dispatchEvent(e);
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback(function() {
  chrome.test.runTests(tests);
});
