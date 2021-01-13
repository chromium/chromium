// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const tests = [
  /**
   * Test that the correct title is displayed for test-whitespace-title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertEq('test-whitespace-title.pdf', document.title);

    chrome.test.succeed();
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCompleteCallback(function() {
  chrome.test.runTests(tests);
});
