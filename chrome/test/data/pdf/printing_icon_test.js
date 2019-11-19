// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests([
  function testPrintingEnabled() {
    const toolbar = document.body.querySelector('#toolbar');
    toolbar.printingEnabled = true;
    const printIcon = toolbar.shadowRoot.querySelector('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertFalse(printIcon.hidden);
    chrome.test.succeed();
  },
  function testPrintingDisabled() {
    const toolbar = document.body.querySelector('#toolbar');
    toolbar.printingEnabled = false;
    const printIcon = toolbar.shadowRoot.querySelector('#print');
    chrome.test.assertTrue(!!printIcon);
    chrome.test.assertTrue(printIcon.hidden);
    chrome.test.succeed();
  },
]);
