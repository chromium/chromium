// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerDocumentOutlineElement, ViewerPdfSidenavElement, ViewerThumbnailBarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

/** @return {!ViewerPdfSidenavElement} */
function createSidenav() {
  document.body.innerHTML = '';
  const sidenav = /** @type {!ViewerPdfSidenavElement} */ (
      document.createElement('viewer-pdf-sidenav'));
  document.body.appendChild(sidenav);
  return sidenav;
}

// Unit tests for the viewer-pdf-sidenav element.
const tests = [
  /**
   * Test that the sidenav toggles between outline and thumbnail view.
   */
  function testViewToggle() {
    const sidenav = createSidenav();

    // Add some dummy bookmarks so that the tabs selectors appear.
    sidenav.bookmarks = [
      {title: 'Foo', page: 1},
      {title: 'Bar', page: 2},
    ];

    const content = sidenav.shadowRoot.querySelector('#content');
    const [thumbnailButton, outlineButton] =
        sidenav.shadowRoot.querySelectorAll('cr-icon-button');

    const thumbnailBar =
        /** @type {!ViewerThumbnailBarElement} */ (
            content.querySelector('viewer-thumbnail-bar'));
    const outline = /** @type {!ViewerDocumentOutlineElement} */ (
        content.querySelector('viewer-document-outline'));


    // Sidebar starts on thumbnail view.
    chrome.test.assertTrue(
        thumbnailButton.parentNode.classList.contains('selected'));
    chrome.test.assertEq('true', thumbnailButton.getAttribute('aria-selected'));
    chrome.test.assertFalse(
        outlineButton.parentNode.classList.contains('selected'));
    chrome.test.assertEq('false', outlineButton.getAttribute('aria-selected'));
    chrome.test.assertFalse(thumbnailBar.hidden);
    chrome.test.assertTrue(outline.hidden);

    // Click on outline view.
    outlineButton.click();
    chrome.test.assertFalse(
        thumbnailButton.parentNode.classList.contains('selected'));
    chrome.test.assertEq(
        'false', thumbnailButton.getAttribute('aria-selected'));
    chrome.test.assertTrue(
        outlineButton.parentNode.classList.contains('selected'));
    chrome.test.assertEq('true', outlineButton.getAttribute('aria-selected'));
    chrome.test.assertTrue(thumbnailBar.hidden);
    chrome.test.assertFalse(outline.hidden);

    // Return to thumbnail view.
    thumbnailButton.click();
    chrome.test.assertTrue(
        thumbnailButton.parentNode.classList.contains('selected'));
    chrome.test.assertEq('true', thumbnailButton.getAttribute('aria-selected'));
    chrome.test.assertFalse(
        outlineButton.parentNode.classList.contains('selected'));
    chrome.test.assertEq('false', outlineButton.getAttribute('aria-selected'));
    chrome.test.assertFalse(thumbnailBar.hidden);
    chrome.test.assertTrue(outline.hidden);

    chrome.test.succeed();
  },

  function testTabIconsHidden() {
    const sidenav = createSidenav();
    const buttonsContainer = sidenav.shadowRoot.querySelector('#icons');

    chrome.test.assertEq(0, sidenav.bookmarks.length);
    chrome.test.assertTrue(buttonsContainer.hidden);

    // Add dummy bookmarks so that the buttons appear.
    sidenav.bookmarks = [
      {title: 'Foo', page: 1},
      {title: 'Bar', page: 2},
    ];

    chrome.test.assertFalse(buttonsContainer.hidden);
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
