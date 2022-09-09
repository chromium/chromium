// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  /**
   * Test that the correct title is displayed for test-title.pdf.
   */
  function testHasCorrectTitle() {
    chrome.test.assertEq('PDF title test', document.title);

    chrome.test.succeed();
  },
]);
