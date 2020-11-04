// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer.js';

const tests = [
  function testFullscreen() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('pdf-viewer'));
    const toolbar = viewer.shadowRoot.querySelector('viewer-pdf-toolbar-new');
    toolbar.dispatchEvent(new CustomEvent('fullscreen-click'));
    eventToPromise('fullscreenchange', viewer).then(e => {
      chrome.test.assertEq(
          viewer.shadowRoot.querySelector('#main'), e.composedPath()[0]);
      chrome.test.succeed();
    });
  },
];

chrome.test.runTests(tests);
