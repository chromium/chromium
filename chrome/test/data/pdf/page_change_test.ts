// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PdfViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

function getViewer(): PdfViewerElement {
  return document.body.querySelector('pdf-viewer')!;
}

function simulateFormFocusChange(focused: boolean) {
  const plugin = getViewer().shadowRoot!.querySelector('embed')!;
  plugin.dispatchEvent(
      new MessageEvent('message', {data: {type: 'formFocusChange', focused}}));
}

function resetDocument() {
  const viewer = getViewer();
  viewer.viewport.goToPage(0);
  simulateFormFocusChange(false);
}

function getCurrentPage(): number {
  return getViewer().viewport.getMostVisiblePage();
}

function getAllPossibleKeyModifiers() {
  const modifiers: Array<string|string[]> = ['shift', 'ctrl', 'alt', 'meta'];
  modifiers.push(
      ['shift', 'ctrl'], ['shift', 'alt'], ['shift', 'meta'], ['ctrl', 'alt'],
      ['ctrl', 'meta'], ['alt', 'meta']);
  modifiers.push(
      ['shift', 'ctrl', 'alt'], ['shift', 'ctrl', 'meta'],
      ['shift', 'alt', 'meta'], ['ctrl', 'alt', 'meta']);
  modifiers.push(['shift', 'ctrl', 'alt', 'meta']);
  return modifiers;
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
   * Test that when the PDF Viewer is in fit-to-page mode:
   *  - Pressing page up/page down changes page back/forth.
   *  - Pressing any modifiers + page up/page down does not.
   */
  function testPageDownInFitPage() {
    getViewer().viewport.fitToPage();

    // Modifiers + Page down -> Does not change the page.
    const modifiers = getAllPossibleKeyModifiers();
    for (const mods of modifiers) {
      pressAndReleaseKeyOn(document.documentElement, 34, mods, 'PageDown');
      chrome.test.assertEq(0, getCurrentPage());
    }

    // Page down -> Go to page 2.
    pressAndReleaseKeyOn(document.documentElement, 34, '', 'PageDown');
    chrome.test.assertEq(1, getCurrentPage());

    // Modifiers + Page up -> Does not change the page.
    for (const mods of modifiers) {
      pressAndReleaseKeyOn(document.documentElement, 33, mods, 'PageUp');
      chrome.test.assertEq(1, getCurrentPage());
    }

    // Page up -> Back to page 1.
    pressAndReleaseKeyOn(document.documentElement, 33, '', 'PageUp');
    chrome.test.assertEq(0, getCurrentPage());

    resetDocument();
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
