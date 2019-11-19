// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFScriptingAPI} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';

// Tests common to all PDFs.
const tests = [
  function testLayoutOptions() {
    chrome.test.assertEq(
        {
          defaultPageOrientation: 0,
        },
        viewer.viewport.getLayoutOptions());
    chrome.test.succeed();
  },
];

// Tests specific to each PDF's layout.
const perLayoutTests = {
  'test-layout3.pdf': [
    function testDimensions3() {
      chrome.test.assertEq(
          {
            width: 103,
            height: 437,
          },
          viewer.viewport.getDocumentDimensions());
      chrome.test.succeed();
    },
  ],

  'test-layout4.pdf': [
    function testDimensions4() {
      chrome.test.assertEq(
          {
            width: 143,
            height: 504,
          },
          viewer.viewport.getDocumentDimensions());
      chrome.test.succeed();
    },
  ],
};

const scriptingAPI = new PDFScriptingAPI(window, window);
scriptingAPI.setLoadCallback((success) => {
  if (success && document.title in perLayoutTests) {
    chrome.test.runTests(tests.concat(perLayoutTests[document.title]));
  } else {
    chrome.test.fail(document.title);
  }
});
