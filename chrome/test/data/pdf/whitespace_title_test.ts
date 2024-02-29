// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {checkPdfTitleIsExpectedTitle} from './test_util.js';

chrome.test.runTests([
  /**
   * Test that the correct title is displayed for test-whitespace-title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertTrue(
        checkPdfTitleIsExpectedTitle('test-whitespace-title.pdf'));

    chrome.test.succeed();
  },
]);
