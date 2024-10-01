// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PdfScriptingApi} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_scripting_api.js';
import {FittingType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {createWheelEvent, ensureFullscreen, enterFullscreenWithUserGesture} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const scroller = viewer.$.scroller;

async function enterAndExitFullscreen(): Promise<void> {
  await enterFullscreenWithUserGesture();
  document.exitFullscreen();
  await eventToPromise('fullscreenchange', scroller);
}

async function assertEnterAndExitFullscreenWithType(fittingType: FittingType):
    Promise<void> {
  // Fullscreen must be exited at the start of this test in order for it to
  // function properly.
  chrome.test.assertTrue(document.fullscreenElement === null);

  viewer.viewport.setFittingType(fittingType);
  await enterAndExitFullscreen();
  chrome.test.assertTrue(document.fullscreenElement === null);
  chrome.test.assertEq(fittingType, viewer.viewport.fittingType);
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

    const content = viewer.$.content;

    // Simulate scrolling towards the bottom.
    content.dispatchEvent(
        createWheelEvent(40, {clientX: 0, clientY: 0}, false));
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    // Simulate scrolling towards the top.
    content.dispatchEvent(
        createWheelEvent(-40, {clientX: 0, clientY: 0}, false));
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    chrome.test.succeed();
  },
  async function testKeysUpdatePage() {
    await ensureFullscreen();
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Test arrow keys.
    keyDownOn(viewer, 0, [], 'ArrowDown');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, [], 'ArrowUp');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, [], 'ArrowRight');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, [], 'ArrowLeft');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Test Space key.
    keyDownOn(viewer, 0, [], ' ');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, 'shift', ' ');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    // Test PageUp/PageDown keys.
    keyDownOn(viewer, 0, [], 'PageDown');
    chrome.test.assertEq(1, viewer.viewport.getMostVisiblePage());

    keyDownOn(viewer, 0, [], 'PageUp');
    chrome.test.assertEq(0, viewer.viewport.getMostVisiblePage());

    chrome.test.succeed();
  },
  async function testZoomKeyboardShortcutsDisabled() {
    await ensureFullscreen();

    async function keydown(key: string) {
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

    const client = new PdfScriptingApi(window, window);
    client.selectAll();
    client.getSelectedText(selectedText => {
      // No text should be selected.
      chrome.test.assertEq(0, selectedText.length);
      chrome.test.succeed();
    });
  },
  async function testFocusAfterExiting() {
    await ensureFullscreen();
    document.exitFullscreen();
    await eventToPromise('fullscreenchange', scroller);
    chrome.test.assertEq('EMBED', getDeepActiveElement()!.nodeName);
    chrome.test.succeed();
  },
  async function testZoomAfterExiting() {
    // Fullscreen must be exited at the start of this test in order for it to
    // function properly.
    chrome.test.assertTrue(document.fullscreenElement === null);

    // Zoom before fullscreen should be restored after entering and exiting
    // fullscreen.
    viewer.viewport.setZoom(0.5);
    await enterAndExitFullscreen();
    chrome.test.assertEq(0.5, viewer.viewport.getZoom());
    chrome.test.assertEq(FittingType.NONE, viewer.viewport.fittingType);

    chrome.test.succeed();
  },
  async function testEnterAndExitFullscreenWithTypeFitToPage() {
    await assertEnterAndExitFullscreenWithType(FittingType.FIT_TO_PAGE);
    chrome.test.succeed();
  },
  async function testEnterAndExitFullscreenWithTypeFitToWidth() {
    await assertEnterAndExitFullscreenWithType(FittingType.FIT_TO_WIDTH);
    chrome.test.succeed();
  },
  async function testEnterAndExitFullscreenWithTypeFitToHeight() {
    await assertEnterAndExitFullscreenWithType(FittingType.FIT_TO_HEIGHT);
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
