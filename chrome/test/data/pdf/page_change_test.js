// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

function resetDocument() {
  window.viewer.viewport.goToPage(0);
  window.viewer.viewport.setZoom(1);
  window.viewer.isFormFieldFocused_ = false;
}

function getCurrentPage() {
  return window.viewer.viewport.getMostVisiblePage();
}

const tests = [
  /**
   * Test that the left/right arrows change page back and forth.
   */
  function testPageChangesWithArrows() {
    // Right arrow -> Go to page 2.
    pressAndReleaseKeyOn(document, 39);
    chrome.test.assertEq(1, getCurrentPage());

    // Left arrow -> Back to page 1.
    pressAndReleaseKeyOn(document, 37);
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
    window.viewer.isFormFieldFocused_ = true;

    // Page should not change when left/right are pressed.
    pressAndReleaseKeyOn(document, 39);
    chrome.test.assertEq(0, getCurrentPage());

    pressAndReleaseKeyOn(document, 37);
    chrome.test.assertEq(0, getCurrentPage());

    resetDocument();
    chrome.test.succeed();
  },

  /**
   * Test that when the document is in fit to page, pressing page up/page down
   * changes page back/forth.
   */
  function testPageDownInFitPage() {
    window.viewer.viewport.fitToPage();

    // Page down -> Go to page 2.
    pressAndReleaseKeyOn(document, 34);
    chrome.test.assertEq(1, getCurrentPage());

    // Page up -> Back to page 1.
    pressAndReleaseKeyOn(document, 33);
    chrome.test.assertEq(0, getCurrentPage());

    resetDocument();
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
