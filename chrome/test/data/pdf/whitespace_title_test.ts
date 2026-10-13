// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {checkPdfTitleIsExpectedTitle} from './test_util.js';

suite('PDFWhitespaceTitleTest', () => {
  /**
   * Test that the correct title is displayed for test-whitespace-title.pdf.
   */
  test('testHasCorrectTitle', () => {
    assertTrue(checkPdfTitleIsExpectedTitle('test-whitespace-title.pdf'));
  });
});
