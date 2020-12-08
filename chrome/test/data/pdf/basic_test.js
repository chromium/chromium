// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilenameFromURL, PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer.js';
import {shouldIgnoreKeyEvents} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_utils.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

const tests = [
  /**
   * Test that some key elements exist and that they have a shadowRoot. This
   * verifies that Polymer is working correctly.
   */
  function testHasElements() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('pdf-viewer'));
    const commonElements = ['viewer-password-screen', 'viewer-error-screen'];
    const elementNames =
        document.documentElement.hasAttribute('pdf-viewer-update-enabled') ?
        ['viewer-pdf-toolbar-new', 'viewer-pdf-sidenav', ...commonElements] :
        ['viewer-pdf-toolbar', 'viewer-zoom-toolbar', ...commonElements];
    for (let i = 0; i < elementNames.length; i++) {
      const elements = viewer.shadowRoot.querySelectorAll(elementNames[i]);
      chrome.test.assertEq(1, elements.length);
      chrome.test.assertTrue(elements[0].shadowRoot !== null);
    }
    chrome.test.succeed();
  },

  /**
   * Test that the plugin element exists and is navigated to the correct URL.
   */
  function testPluginElement() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('pdf-viewer'));
    const plugin = viewer.shadowRoot.querySelector('#plugin');
    chrome.test.assertEq('embed', plugin.localName);

    chrome.test.assertTrue(
        plugin.getAttribute('src').indexOf('/pdf/test.pdf') !== -1);
    chrome.test.succeed();
  },

  function testShouldIgnoreKeyEvents() {
    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('pdf-viewer'));
    const toolbar = /** @type {!ViewerPdfToolbarElement} */ (
        viewer.shadowRoot.querySelector('#toolbar'));

    // Test case where an <input> field is focused.
    toolbar.shadowRoot.querySelector('viewer-page-selector')
        .pageSelector.focus();
    chrome.test.assertTrue(shouldIgnoreKeyEvents());

    // Test case where another field is focused.
    const rotateButton =
        document.documentElement.hasAttribute('pdf-viewer-update-enabled') ?
        toolbar.shadowRoot.querySelector(
            'cr-icon-button[iron-icon=\'pdf:rotate-left\']') :
        toolbar.$['rotate-right'];
    rotateButton.focus();
    chrome.test.assertFalse(shouldIgnoreKeyEvents());

    // Test case where the plugin itself is focused.
    viewer.shadowRoot.querySelector('#plugin').focus();
    chrome.test.assertFalse(shouldIgnoreKeyEvents());

    chrome.test.succeed();
  },

  /**
   * Test that the bookmarks menu can be closed by clicking the plugin and
   * pressing escape.
   */
  function testOpenCloseBookmarks() {
    // Test is not relevant for the new viewer, as bookmarks are no longer in a
    // dropdown.
    if (document.documentElement.hasAttribute('pdf-viewer-update-enabled')) {
      chrome.test.succeed();
      return;
    }

    const viewer = /** @type {!PDFViewerElement} */ (
        document.body.querySelector('pdf-viewer'));
    const toolbar = /** @type {!ViewerPdfToolbarElement} */ (
        viewer.shadowRoot.querySelector('#toolbar'));
    toolbar.show();
    const dropdown =
        /** @type {!ViewerToolbarDropdownElement} */ (toolbar.$$('#bookmarks'));
    const plugin = viewer.shadowRoot.querySelector('#plugin');
    const ESC_KEY = 27;

    // Clicking on the plugin should close the bookmarks menu.
    chrome.test.assertFalse(dropdown.dropdownOpen);
    dropdown.$.button.click();
    chrome.test.assertTrue(dropdown.dropdownOpen);
    // Generate pointer event manually, as MockInteractions doesn't include
    // this.
    plugin.dispatchEvent(
        new PointerEvent('pointerdown', {bubbles: true, composed: true}));
    chrome.test.assertFalse(
        dropdown.dropdownOpen, 'Clicking plugin closes dropdown');

    dropdown.$.button.click();
    chrome.test.assertTrue(dropdown.dropdownOpen);
    pressAndReleaseKeyOn(document.documentElement, ESC_KEY, '', 'Escape');
    chrome.test.assertFalse(
        dropdown.dropdownOpen, 'Escape key closes dropdown');
    chrome.test.assertTrue(
        toolbar.opened, 'First escape key does not close toolbar');

    pressAndReleaseKeyOn(document.documentElement, ESC_KEY, '', 'Escape');
    chrome.test.assertFalse(toolbar.opened, 'Second escape key closes toolbar');

    chrome.test.succeed();
  },

  /**
   * Test that the PDF filename is correctly extracted from URLs with query
   * parameters and fragments.
   */
  function testGetFilenameFromURL(url) {
    chrome.test.assertEq(
        'path.pdf',
        getFilenameFromURL(
            'http://example/com/path/with/multiple/sections/path.pdf'));

    chrome.test.assertEq(
        'fragment.pdf',
        getFilenameFromURL('http://example.com/fragment.pdf#zoom=100/Title'));

    chrome.test.assertEq(
        'query.pdf', getFilenameFromURL('http://example.com/query.pdf?p=a/b'));

    chrome.test.assertEq(
        'both.pdf',
        getFilenameFromURL('http://example.com/both.pdf?p=a/b#zoom=100/Title'));

    chrome.test.assertEq(
        'name with spaces.pdf',
        getFilenameFromURL('http://example.com/name%20with%20spaces.pdf'));

    chrome.test.assertEq(
        'invalid%EDname.pdf',
        getFilenameFromURL('http://example.com/invalid%EDname.pdf'));

    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
