// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

// Tests common to all PDFs.
const tests = [
  function testLayoutOptions() {
    const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
    chrome.test.assertEq(
        {
          direction: 2,
          defaultPageOrientation: 0,
          twoUpViewEnabled: false,
        },
        viewer.viewport.getLayoutOptions());
    chrome.test.succeed();
  },
];

// Tests specific to each PDF's layout.
const perLayoutTests: {[name: string]: Array<() => void>} = {
  'test-layout3.pdf': [
    function testDimensions3() {
      const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
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
      const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
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

const viewer = document.body.querySelector<PdfViewerElement>('#viewer')!;
if (viewer.pdfTitle in perLayoutTests) {
  chrome.test.runTests(tests.concat(perLayoutTests[viewer.pdfTitle]!));
} else {
  chrome.test.fail(viewer.pdfTitle);
}
