// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/constants.js';
import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer.js';

const tests = [
  async function testFullscreen() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('pdf-viewer'));
    const scroller = /** @type {!HTMLElement} */ (
        viewer.shadowRoot.querySelector('#scroller'));
    chrome.test.assertTrue(scroller !== null);

    chrome.test.assertEq(FittingType.NONE, viewer.viewport.fittingType);

    const whenFitToTypeChanged =
        eventToPromise('fitting-type-changed-for-testing', scroller);
    const whenFullscreenChange = eventToPromise('fullscreenchange', scroller);

    const toolbar = viewer.shadowRoot.querySelector('viewer-pdf-toolbar-new');
    toolbar.dispatchEvent(new CustomEvent('fullscreen-click'));
    await whenFitToTypeChanged;
    await whenFullscreenChange;

    // Check that the scrollbars are hidden.
    chrome.test.assertEq('hidden', window.getComputedStyle(scroller).overflow);
    // Check that the `fittingType` has changed.
    chrome.test.assertEq(
        FittingType.FIT_TO_HEIGHT, viewer.viewport.fittingType);

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
