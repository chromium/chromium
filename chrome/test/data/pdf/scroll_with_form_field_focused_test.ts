// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const scroller = viewer.$.scroller;

function simulateFormFocusChange(focused: boolean) {
  const plugin = viewer.shadowRoot!.querySelector('embed')!;
  plugin.dispatchEvent(
      new MessageEvent('message', {data: {type: 'formFocusChange', focused}}));
}

function resetDocumentAndFocusOnForm() {
  viewer.viewport.fitToNone();
  viewer.viewport.goToPage(0);
  simulateFormFocusChange(true);
}

function getCurrentPage(): number {
  return viewer.viewport.getMostVisiblePage();
}

const tests = [
  // Scroll with PageDown key when a form field is in focus.
  async function testPageDownScrolling() {
    resetDocumentAndFocusOnForm();

    const whenScrollProceeded =
        eventToPromise('scroll-proceeded-for-testing', scroller);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'PageDown'}));
    await whenScrollProceeded;

    chrome.test.succeed();
  },

  // Scroll with PageUp key when a form field is in focus.
  async function testPageUpScrolling() {
    resetDocumentAndFocusOnForm();
    viewer.viewport.goToPage(1);

    const whenScrollProceeded =
        eventToPromise('scroll-proceeded-for-testing', scroller);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'PageUp'}));
    await whenScrollProceeded;

    chrome.test.succeed();
  },

  // Test that when a PDF form field is in focus, the space key shortcut won't
  // trigger scrolling.
  async function testSpaceKeyScrolling() {
    resetDocumentAndFocusOnForm();

    // Scrolling should not happen when the space key is pressed.
    const whenScrollAvoided =
        eventToPromise('scroll-avoided-for-testing', scroller);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    await whenScrollAvoided;

    chrome.test.succeed();
  },

  // Test that when a PDF form field is in focus, the shift + space shortcut
  // won't trigger scrolling.
  async function testShiftSpaceKeyScrolling() {
    resetDocumentAndFocusOnForm();
    viewer.viewport.goToPage(1);

    // Scrolling should not happen when the shift + space keys are pressed.
    const whenScrollAvoided =
        eventToPromise('scroll-avoided-for-testing', scroller);
    document.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', shiftKey: true}));
    await whenScrollAvoided;

    chrome.test.succeed();
  },

  // Test that when the PDF is in fit-to-page and a form field is in focus,
  // PageUp/PageDown will trigger page changes.
  function testPageUpDownScrollingInPageMode() {
    resetDocumentAndFocusOnForm();
    viewer.viewport.fitToPage();

    // Page down -> Go to page 2.
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'PageDown'}));
    chrome.test.assertEq(1, getCurrentPage());

    // Page up -> Back to page 1.
    document.dispatchEvent(new KeyboardEvent('keydown', {key: 'PageUp'}));
    chrome.test.assertEq(0, getCurrentPage());

    chrome.test.succeed();
  },

  // Test that when the PDF is in fit-to-page and the form field is in focus,
  // the space shortcut won't trigger scrolling.
  async function testSpaceKeyScrollingInPageMode() {
    resetDocumentAndFocusOnForm();
    viewer.viewport.fitToPage();

    // Scrolling should not happen when the space key is pressed.
    const whenScrollAvoided =
        eventToPromise('scroll-avoided-for-testing', scroller);
    document.dispatchEvent(new KeyboardEvent('keydown', {key: ' '}));
    await whenScrollAvoided;

    chrome.test.succeed();
  },

  // Test that when the PDF is in fit-to-page and the form field is in focus,
  // the shift + space shortcut won't trigger scrolling.
  async function testShiftSpaceKeyScrollingInPageMode() {
    resetDocumentAndFocusOnForm();
    viewer.viewport.fitToPage();
    viewer.viewport.goToPage(1);

    // Scrolling should not happen when the shift + space keys are pressed.
    const whenScrollAvoided =
        eventToPromise('scroll-avoided-for-testing', scroller);
    document.dispatchEvent(
        new KeyboardEvent('keydown', {key: ' ', shiftKey: true}));
    await whenScrollAvoided;

    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
