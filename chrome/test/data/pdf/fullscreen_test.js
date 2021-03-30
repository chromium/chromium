// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {FittingType, PDFScriptingAPI, PDFViewerElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isMac} from 'chrome://resources/js/cr.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {createWheelEvent} from './test_util.js';

const viewer = /** @type {!PDFViewerElement} */ (
    document.body.querySelector('pdf-viewer'));
const scroller =
    /** @type {!HTMLElement} */ (viewer.shadowRoot.querySelector('#scroller'));

/** @return {!Promise<void>} */
async function ensureFullscreen() {
  if (document.fullscreenElement !== null) {
    return;
  }

  const toolbar = viewer.shadowRoot.querySelector('viewer-pdf-toolbar-new');
  toolbar.dispatchEvent(new CustomEvent('present-click'));
  await eventToPromise('fullscreenchange', scroller);
}

const tests = [
  async function testFullscreen() {
    chrome.test.assertTrue(scroller !== null);
    chrome.test.assertEq(null, document.fullscreenElement);
    chrome.test.assertEq(FittingType.NONE, viewer.viewport.fittingType);

    const whenFitToTypeChanged =
        eventToPromise('fitting-type-changed-for-testing', scroller);

    await ensureFullscreen();
    await whenFitToTypeChanged;

    // Check that the scrollbars are hidden.
    chrome.test.assertEq('hidden', window.getComputedStyle(scroller).overflow);
    // Check that the `fittingType` has changed.
    chrome.test.assertEq(
        FittingType.FIT_TO_HEIGHT, viewer.viewport.fittingType);

    chrome.test.succeed();
  },
  async function testRotateKeyboardShortcutsDisabled() {
    await ensureFullscreen();
    chrome.test.assertEq(0, viewer.viewport.getClockwiseRotations());
    keyDownOn(viewer, 0, 'ctrl', '[');
    chrome.test.assertEq(0, viewer.viewport.getClockwiseRotations());
    keyDownOn(viewer, 0, 'ctrl', ']');
    chrome.test.assertEq(0, viewer.viewport.getClockwiseRotations());
    chrome.test.succeed();
  },
  async function testWheelEventUpdatesPage() {
    await ensureFullscreen();
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Simulate scrolling towards the bottom.
    scroller.dispatchEvent(
        createWheelEvent(40, {clientX: 0, clientY: 0}, false));
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    // Simulate scrolling towards the top.
    scroller.dispatchEvent(
        createWheelEvent(-40, {clientX: 0, clientY: 0}, false));
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    chrome.test.succeed();
  },
  async function testKeysUpdatePage() {
    await ensureFullscreen();
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Test arrow keys.
    keyDownOn(viewer, 0, '', 'ArrowDown');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, '', 'ArrowUp');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, '', 'ArrowRight');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, '', 'ArrowLeft');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Test Space key.
    keyDownOn(viewer, 0, '', ' ');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, 'shift', ' ');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Test PageUp/PageDown keys.
    keyDownOn(viewer, 0, '', 'PageDown');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, '', 'PageUp');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    chrome.test.succeed();
  },
  async function testZoomKeyboardShortcutsDisabled() {
    await ensureFullscreen();

    async function keydown(key) {
      const whenKeydown = eventToPromise('keydown', viewer);
      keyDownOn(viewer, 0, isMac ? 'meta' : 'ctrl', key);
      return await whenKeydown;
    }

    // Test case where the '+' button (which co-resides with the '=' button) is
    // pressed.
    let e = await keydown('=');
    chrome.test.assertTrue(e.defaultPrevented);

    // Test case where the '-' button is pressed.
    e = await keydown('-');
    chrome.test.assertTrue(e.defaultPrevented);

    // Test case where the '+' button (in the numpad) is pressed.
    e = await keydown('+');
    chrome.test.assertTrue(e.defaultPrevented);

    chrome.test.succeed();
  },
  async function testTextSelectionDisabled() {
    await ensureFullscreen();

    const client = new PDFScriptingAPI(window, window);
    client.selectAll();
    client.getSelectedText(selectedText => {
      // No text should be selected.
      chrome.test.assertEq(0, selectedText.length);
      chrome.test.succeed();
    });
  },
  // Note: The following test needs to be the last one, because subsequent calls
  // to requestFullScreen() fail with an "API can only be initiated by a user
  // gesture" error.
  async function testFocusAfterExiting() {
    await ensureFullscreen();
    document.exitFullscreen();
    await eventToPromise('fullscreenchange', scroller);
    chrome.test.assertEq('EMBED', getDeepActiveElement().nodeName);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
