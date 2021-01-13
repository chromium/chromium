// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI, PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const tests = [
  /**
   * Test that blocked JS was not able to call back via "app.beep()"
   */
  function testHasCorrectBeepCount() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('#viewer'));
    chrome.test.assertEq(0, viewer.beepCount);
    chrome.test.succeed();
  }
];

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCompleteCallback(function() {
  chrome.test.runTests(tests);
});
