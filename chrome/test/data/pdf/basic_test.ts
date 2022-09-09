// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getFilenameFromURL, shouldIgnoreKeyEvents} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

const tests = [
  /**
   * Test that some key elements exist and that they have a shadowRoot. This
   * verifies that Polymer is working correctly.
   */
  function testHasElements() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const elementNames = ['viewer-pdf-sidenav', 'viewer-toolbar'];

    for (const elementName of elementNames) {
      const elements = viewer.shadowRoot!.querySelectorAll(elementName);
      chrome.test.assertEq(1, elements.length);
      chrome.test.assertTrue(elements[0]!.shadowRoot !== null);
    }
    chrome.test.succeed();
  },

  /**
   * Test that the plugin element exists and is navigated to the correct URL.
   */
  function testPluginElement() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const plugin = viewer.shadowRoot!.querySelector('#plugin')!;
    chrome.test.assertEq('embed', plugin.localName);

    chrome.test.assertTrue(
        plugin.getAttribute('original-url')!.indexOf('/pdf/test.pdf') !== -1);
    chrome.test.succeed();
  },

  function testShouldIgnoreKeyEvents() {
    const viewer = document.body.querySelector('pdf-viewer')!;
    const toolbar = viewer.$.toolbar;

    // Test case where an <input> field is focused.
    toolbar.shadowRoot!.querySelector(
                           'viewer-page-selector')!.$.pageSelector.focus();
    chrome.test.assertTrue(shouldIgnoreKeyEvents());

    // Test case where another field is focused.
    const rotateButton = toolbar.shadowRoot!.querySelector<HTMLElement>(
        'cr-icon-button[iron-icon=\'pdf:rotate-left\']')!;
    rotateButton.focus();
    chrome.test.assertFalse(shouldIgnoreKeyEvents());

    // Test case where the plugin itself is focused.
    viewer.shadowRoot!.querySelector<HTMLElement>('#plugin')!.focus();
    chrome.test.assertFalse(shouldIgnoreKeyEvents());

    chrome.test.succeed();
  },

  /**
   * Test that the PDF filename is correctly extracted from URLs with query
   * parameters and fragments.
   */
  function testGetFilenameFromURL() {
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
  },
];

chrome.test.runTests(tests);
