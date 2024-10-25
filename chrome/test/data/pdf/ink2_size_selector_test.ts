// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {InkSizeSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertSelectedSize, getSizeButtons} from './test_util.js';

function createSelector(): InkSizeSelectorElement {
  const selector = document.createElement('ink-size-selector');
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
    sizeButtons: NodeListOf<HTMLElement>, targetElement: HTMLElement,
    key: string, expectedButtonIndex: number) {
  keyDownOn(targetElement, 0, [], key);
  await microtasksFinished();

  assertSelectedSize(sizeButtons, /*buttonIndex=*/ expectedButtonIndex);
}

chrome.test.runTests([
  // Test that clicking the size buttons changes the selected size.
  async function testClick() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    sizeButtons[0].click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 0);

    sizeButtons[1].click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 1);
    chrome.test.succeed();
  },

  // Test that arrow keys do nothing to the size when the size buttons are not
  // focused.
  async function testArrowKeysSizeNonFocused() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    sizeButtons[4].click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 4);

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

    sizeButtons[4].click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 4);

    // Pressing 'ArrowLeft' or 'ArrowUp' should select the previous size button.
    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[4], 'ArrowLeft', /*expectedButtonIndex=*/ 3);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[3], 'ArrowUp', /*expectedButtonIndex=*/ 2);

    // Pressing 'ArrowRight' or 'ArrowDown' should select the next size button.
    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[2], 'ArrowRight', /*expectedButtonIndex=*/ 3);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[3], 'ArrowDown', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test that when the last size button is selected, pressing 'ArrowRight' or
  // 'ArrowDown' will select the first size button.
  // Test that when the first size button is selected, pressing 'ArrowLeft' or
  // 'ArrowUp' will select the last size button.
  async function testArrowKeysChangeSizeFirstLast() {
    const selector = createSelector();
    const sizeButtons = getSizeButtons(selector);

    sizeButtons[4].click();
    await microtasksFinished();

    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[4], 'ArrowRight', /*expectedButtonIndex=*/ 0);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[0], 'ArrowLeft', /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[4], 'ArrowDown', /*expectedButtonIndex=*/ 0);

    await testSizeKeyboardEvent(
        sizeButtons, sizeButtons[0], 'ArrowUp', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },
]);
