// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

chrome.test.runTests([
  /**
   * Test that blocked JS was not able to call back via "app.beep()"
   */
  function testHasCorrectBeepCount() {
    const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
    chrome.test.assertEq(0, viewer.beepCount);
    chrome.test.succeed();
  },
]);
