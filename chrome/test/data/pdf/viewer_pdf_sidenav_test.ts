// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ViewerPdfSidenavElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

function createSidenav(): ViewerPdfSidenavElement {
  document.body.innerHTML = '';
  const sidenav = document.createElement('viewer-pdf-sidenav');
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
      {title: 'Foo', page: 1, children: []},
      {title: 'Bar', page: 2, children: []},
    ];

    const icons = sidenav.shadowRoot!.querySelector('#icons')!;
    const content = sidenav.shadowRoot!.querySelector('#content')!;
    const buttons = sidenav.shadowRoot!.querySelectorAll('cr-icon-button');
    const thumbnailButton = buttons[0]!;
    const outlineButton = buttons[1]!;

    const thumbnailBar = content.querySelector('viewer-thumbnail-bar')!;
    const outline = content.querySelector('viewer-document-outline')!;

    function assertThumbnailView() {
      chrome.test.assertTrue(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertEq(
          'true', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertFalse(
          outlineButton.parentElement!.classList.contains('selected'));
      chrome.test.assertEq(
          'false', outlineButton.getAttribute('aria-selected'));
      chrome.test.assertFalse(thumbnailBar.hidden);
      chrome.test.assertTrue(outline.hidden);
    }

    function assertOutlineView() {
      chrome.test.assertFalse(
          thumbnailButton.parentElement!.classList.contains('selected'));
      chrome.test.assertEq(
          'false', thumbnailButton.getAttribute('aria-selected'));
      chrome.test.assertTrue(
          outlineButton.parentElement!.classList.contains('selected'));
      chrome.test.assertEq('true', outlineButton.getAttribute('aria-selected'));
      chrome.test.assertTrue(thumbnailBar.hidden);
      chrome.test.assertFalse(outline.hidden);
    }


    // Sidebar starts on thumbnail view.
    assertThumbnailView();

    // Click on outline view.
    outlineButton.click();
    assertOutlineView();

    // Return to thumbnail view.
    thumbnailButton.click();
    assertThumbnailView();

    // Arrow keys toggle between thumbnail and outline view.

    // Thumbnail -> Outline
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertOutlineView();

    // Outline -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowDown');
    assertThumbnailView();

    // Thumbnail -> Outline
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertOutlineView();

    // Outline -> Thumbnail
    keyDownOn(icons, 0, '', 'ArrowUp');
    assertThumbnailView();

    // Pressing arrow keys outside of icons shouldn't do anything.
    keyDownOn(content, 0, '', 'ArrowDown');
    assertThumbnailView();

    chrome.test.succeed();
  },

  function testTabIconsHidden() {
    const sidenav = createSidenav();
    const buttonsContainer =
        sidenav.shadowRoot!.querySelector<HTMLElement>('#icons')!;

    chrome.test.assertEq(0, sidenav.bookmarks.length);
    chrome.test.assertTrue(buttonsContainer.hidden);

    // Add dummy bookmarks so that the buttons appear.
    sidenav.bookmarks = [
      {title: 'Foo', page: 1, children: []},
      {title: 'Bar', page: 2, children: []},
    ];

    chrome.test.assertFalse(buttonsContainer.hidden);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
