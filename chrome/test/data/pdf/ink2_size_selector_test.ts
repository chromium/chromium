// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InkSizeSelectorElement, SelectableIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {PEN_SIZES} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertSelectedSize, getSizeButtons} from './test_util.js';

function createSelector(initialValue?: number): InkSizeSelectorElement {
  const selector = document.createElement('ink-size-selector');
  // Emulate the parent initializing this value via a data binding, e.g. before
  // the sidepanel is shown, or before the bottom toolbar size button is
  // clicked to add this element to the DOM.
  if (initialValue) {
    selector.currentSize = initialValue;
  }
  document.body.innerHTML = '';
  document.body.appendChild(selector);
  return selector;
}

/**
 * Tests that the selected size index is `expectedButtonIndex` after
 * `targetElement` receives a keyboard event with key `key`.
 * @param sizeButtons The list of ink size buttons.
 * @param targetElement The target element to receive the keyboard event.
 * @param key The key being pressed.
 * @param expectedButtonIndex The expected size button index after the keyboard
 *     event.
 */
async function testSizeKeyboardEvent(
    sizeButtons: NodeListOf<SelectableIconButtonElement>,
    targetElement: HTMLElement, key: string, expectedButtonIndex: number) {
  keyDownOn(targetElement, 0, [], key);
  await microtasksFinished();

  assertSelectedSize(sizeButtons, /*buttonIndex=*/ expectedButtonIndex);
  assertTabIndices(sizeButtons, /*buttonIndex=*/ expectedButtonIndex);
}

/**
 * Tests that the ink size options have correct tab indices. The size button
 * with index `buttonIndex` should have a tabindex of 0. The remaining buttons
 * should have a tabindex of -1.
 * @sizeButtons A list of ink size buttons.
 * @param buttonIndex The expected size button with a tabindex of 0.
 */
function assertTabIndices(
    sizeButtons: NodeListOf<SelectableIconButtonElement>, buttonIndex: number) {
  for (let i = 0; i < sizeButtons.length; ++i) {
    const actualTabIndex = sizeButtons[i]!.$.button.getAttribute('tabindex');
    chrome.test.assertTrue(actualTabIndex !== null);
    chrome.test.assertEq(i === buttonIndex ? '0' : '-1', actualTabIndex);
  }
}

chrome.test.runTests([
  // Test that clicking the size buttons changes the selected size.
  async function testClick() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    let button = sizeButtons[0];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 0);
    assertTabIndices(sizeButtons, /*buttonIndex=*/ 0);

    button = sizeButtons[1];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 1);
    assertTabIndices(sizeButtons, /*buttonIndex=*/ 1);
    chrome.test.succeed();
  },

  // Test that arrow keys do nothing to the size when the size buttons are not
  // focused.
  async function testArrowKeysSizeNonFocused() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    const button = sizeButtons[4];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 4);
    assertTabIndices(sizeButtons, /*buttonIndex=*/ 4);

    // Press arrow keys on the root element. This should not change the size.
    await testSizeKeyboardEvent(
        sizeButtons, document.documentElement, 'ArrowLeft',
        /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, document.documentElement, 'ArrowUp',
        /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, document.documentElement, 'ArrowRight',
        /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, document.documentElement, 'ArrowDown',
        /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test that arrow keys change brush sizes when the brush size buttons are
  // focused.
  async function testArrowKeysChangeSize() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    const button = sizeButtons[4];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 4);
    assertTabIndices(sizeButtons, /*buttonIndex=*/ 4);

    // Pressing 'ArrowLeft' or 'ArrowUp' should select the previous size button.
    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[4]!, 'ArrowLeft', /*expectedButtonIndex=*/ 3);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[3]!, 'ArrowUp', /*expectedButtonIndex=*/ 2);

    // Pressing 'ArrowRight' or 'ArrowDown' should select the next size button.
    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[2]!, 'ArrowRight', /*expectedButtonIndex=*/ 3);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[3]!, 'ArrowDown', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test that when the last size button is selected, pressing 'ArrowRight' or
  // 'ArrowDown' will select the first size button.
  // Test that when the first size button is selected, pressing 'ArrowLeft' or
  // 'ArrowUp' will select the last size button.
  async function testArrowKeysChangeSizeFirstLast() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    const button = sizeButtons[4];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 4);
    assertTabIndices(sizeButtons, /*buttonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[4]!, 'ArrowRight', /*expectedButtonIndex=*/ 0);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[0]!, 'ArrowLeft', /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[4]!, 'ArrowDown', /*expectedButtonIndex=*/ 0);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[0]!, 'ArrowUp', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test the labels for each size button.
  function testLabels() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    chrome.test.assertEq(sizeButtons[0]!.label, 'Extra thin');
    chrome.test.assertEq(sizeButtons[1]!.label, 'Thin');
    chrome.test.assertEq(sizeButtons[2]!.label, 'Medium');
    chrome.test.assertEq(sizeButtons[3]!.label, 'Thick');
    chrome.test.assertEq(sizeButtons[4]!.label, 'Extra thick');

    chrome.test.succeed();
  },

  async function testFocusesSelectedItem() {
    let selector = createSelector(PEN_SIZES[1]!.size);
    let sizeButtons = getSizeButtons(selector);
    chrome.test.assertEq(5, sizeButtons.length);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 1);
    let whenFocused = eventToPromise('focus', sizeButtons[1]!);
    selector.focus();
    await whenFocused;

    // Recreate the selector and test a different initial condition.
    selector = createSelector(PEN_SIZES[3]!.size);
    sizeButtons = getSizeButtons(selector);
    chrome.test.assertEq(5, sizeButtons.length);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 3);
    whenFocused = eventToPromise('focus', sizeButtons[3]!);
    selector.focus();
    await whenFocused;
    chrome.test.succeed();
  },
]);
