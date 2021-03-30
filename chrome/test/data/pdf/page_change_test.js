// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

/** @return {!PDFViewerElement} */
function getViewer() {
  return /** @type {!PDFViewerElement} */ (
      document.body.querySelector('pdf-viewer'));
}

/** @param {boolean} focused */
function simulateFormFocusChange(focused) {
  const plugin = getViewer().shadowRoot.querySelector('embed');
  plugin.dispatchEvent(
      new MessageEvent('message', {data: {type: 'formFocusChange', focused}}));
}

function resetDocument() {
  const viewer = getViewer();
  viewer.viewport.goToPage(0);
  simulateFormFocusChange(false);
}


/** @return {number} */
function getCurrentPage() {
  return getViewer().viewport.getMostVisiblePage();
}

const tests = [
  /**
   * Test that the left/right arrows change page back and forth.
   */
  function testPageChangesWithArrows() {
    // Right arrow -> Go to page 2.
    pressAndReleaseKeyOn(document.documentElement, 39, '', 'ArrowRight');
    chrome.test.assertEq(1, getCurrentPage());

    // Left arrow -> Back to page 1.
    pressAndReleaseKeyOn(document.documentElement, 37, '', 'ArrowLeft');
    chrome.test.assertEq(0, getCurrentPage());

    resetDocument();
    chrome.test.succeed();
  },

  /**
   * Test that when a PDF form field is focused, the left/right shortcuts are
   * disabled. This doesn't test the plugin side of this feature.
   */
  function testPageDoesntChangeWhenFormFocused() {
    // This should be set by a message from plugin -> page when a field is
    // focused.
    simulateFormFocusChange(true);

    // Page should not change when left/right are pressed.
    pressAndReleaseKeyOn(document.documentElement, 39, '', 'ArrowLeft');
    chrome.test.assertEq(0, getCurrentPage());

    pressAndReleaseKeyOn(document.documentElement, 37, '', 'ArrowRight');
    chrome.test.assertEq(0, getCurrentPage());

    resetDocument();
    chrome.test.succeed();
  },

  /**
   * Test that when the document.documentElement is in fit to page, pressing
   * page up/page down changes page back/forth.
   */
  function testPageDownInFitPage() {
    getViewer().viewport.fitToPage();

    // Page down -> Go to page 2.
    pressAndReleaseKeyOn(document.documentElement, 34, '', 'PageDown');
    chrome.test.assertEq(1, getCurrentPage());

    // Page up -> Back to page 1.
    pressAndReleaseKeyOn(document.documentElement, 33, '', 'PageUp');
    chrome.test.assertEq(0, getCurrentPage());

    resetDocument();
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
