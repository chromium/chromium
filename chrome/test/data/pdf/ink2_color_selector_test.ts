// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {HIGHLIGHTER_COLORS, PEN_COLORS} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {Color, InkColorSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertLabels, assertSelectedColor, getColorButtons} from './test_util.js';


function createSelector(initialValue?: Color): InkColorSelectorElement {
  const selector = document.createElement('ink-color-selector');
  selector.colors = PEN_COLORS;
  // Emulate the parent initializing this value via a data binding, e.g. before
  // the sidepanel is shown, or before the bottom toolbar color button is
  // clicked to add this element to the DOM.
  if (initialValue) {
    selector.currentColor = initialValue;
  }
  document.body.innerHTML = '';
  document.body.appendChild(selector);
  return selector;
}

/**
 * Tests that the selected color index is `expectedButtonIndex` after
 * `targetElement` receives a keyboard event with key `key`.
 * @param colorButtons The list of ink color buttons.
 * @param targetElement The target element to receive the keyboard event.
 * @param key The key being pressed.
 * @param expectedButtonIndex The expected color button index after the keyboard
 *     event.
 */
async function testColorKeyboardEvent(
    colorButtons: NodeListOf<HTMLElement>, targetElement: HTMLElement,
    key: string, expectedButtonIndex: number) {
  keyDownOn(targetElement, 0, [], key);
  await microtasksFinished();

  assertSelectedColor(colorButtons, /*buttonIndex=*/ expectedButtonIndex);
}

chrome.test.runTests([
  // Test that clicking the color buttons changes the selected color.
  async function testClick() {
    const selector = createSelector();
    const colorButtons = getColorButtons(selector);

    const button = colorButtons[6];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedColor(colorButtons, /*buttonIndex=*/ 6);
    chrome.test.succeed();
  },

  // Test that changing the color options updates the DOM correctly.
  async function testColorLength() {
    const selector = createSelector();

    // Pens should have 20 color options.
    await microtasksFinished();
    chrome.test.assertEq(20, getColorButtons(selector).length);

    // Highlighters should have 10 color options.
    selector.colors = HIGHLIGHTER_COLORS;
    await microtasksFinished();

    chrome.test.assertEq(10, getColorButtons(selector).length);
    chrome.test.succeed();
  },

  // Test that arrow keys does nothing to the color when the color buttons are
  // not focused.
  async function testArrowKeysColorNonFocused() {
    const selector = createSelector();
    const colorButtons = getColorButtons(selector);

    const button = colorButtons[2];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedColor(colorButtons, /*buttonIndex=*/ 2);

    // Press arrow keys on the root element. This should not change the size.
    await testColorKeyboardEvent(
        colorButtons, document.documentElement, 'ArrowLeft',
        /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        colorButtons, document.documentElement, 'ArrowRight',
        /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        colorButtons, document.documentElement, 'ArrowUp',
        /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        colorButtons, document.documentElement, 'ArrowDown',
        /*expectedButtonIndex=*/ 2);

    chrome.test.succeed();
  },

  // Test that left and right arrow keys change brush colors when the brush
  // color buttons are focused.
  async function testArrowKeysChangeColor() {
    const selector = createSelector();
    const colorButtons = getColorButtons(selector);

    const button = colorButtons[2];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedColor(colorButtons, /*buttonIndex=*/ 2);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[2]!, 'ArrowLeft',
        /*expectedButtonIndex=*/ 1);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[1]!, 'ArrowRight',
        /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[2]!, 'ArrowDown',
        /*expectedButtonIndex=*/ 7);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[7]!, 'ArrowUp', /*expectedButtonIndex=*/ 2);

    chrome.test.succeed();
  },

  // Test that when the color button in the first column is selected, pressing
  // 'ArrowLeft' will select the color button in the last column in the previous
  // row.
  // Test that when the color button in the last column is selected,
  // pressing 'ArrowRight' will select the color button in the first column in
  // the next row.
  async function testArrowKeysChangeColorFirstLastColumn() {
    const selector = createSelector();
    const colorButtons = getColorButtons(selector);

    const button = colorButtons[5];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedColor(colorButtons, /*buttonIndex=*/ 5);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[5]!, 'ArrowLeft',
        /*expectedButtonIndex=*/ 4);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[9]!, 'ArrowRight',
        /*expectedButtonIndex=*/ 10);

    chrome.test.succeed();
  },

  // Test that when the color button in the first row is selected, pressing
  // 'ArrowUp' will select the color button in the last row in the same column.
  // Test that when the color button in the last row is selected,
  // pressing 'ArrowDown' will select the color button in the first row in the
  // same column.
  async function testArrowKeysChangeColorFirstLastRow() {
    // Defaults to the 20 PEN_COLORS in createSelector().
    const selector = createSelector();
    const colorButtons = getColorButtons(selector);
    chrome.test.assertEq(20, colorButtons.length);

    const button = colorButtons[1];
    chrome.test.assertTrue(!!button);
    button.click();
    await microtasksFinished();

    assertSelectedColor(colorButtons, /*buttonIndex=*/ 1);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[1]!, 'ArrowUp', /*expectedButtonIndex=*/ 16);

    await testColorKeyboardEvent(
        colorButtons, colorButtons[16]!, 'ArrowDown',
        /*expectedButtonIndex=*/ 1);

    chrome.test.succeed();
  },

  // Test the labels when the colors are set to the HIGHLIGHTER_COLORS.
  async function testHighlighterLabels() {
    const selector = createSelector();
    selector.colors = HIGHLIGHTER_COLORS;
    await microtasksFinished();

    const colorButtons = getColorButtons(selector);

    const expectedLabels: string[] = [
      'Light Red',
      'Light Yellow',
      'Light Green',
      'Light Blue',
      'Light Orange',
      'Red',
      'Yellow',
      'Green',
      'Blue',
      'Orange',
    ];
    chrome.test.assertEq(expectedLabels.length, colorButtons.length);

    for (let i = 0; i < colorButtons.length; ++i) {
      assertLabels(colorButtons[i]!, expectedLabels[i]!);
    }

    chrome.test.succeed();
  },

  // Test the labels when the colors are set to the PEN_COLORS (default in
  // createSelector()).
  function testPenLabels() {
    const selector = createSelector();
    const colorButtons = getColorButtons(selector);

    const expectedLabels: string[] = [
      'Black', 'Dark Grey 2', 'Dark Grey 1', 'Light Grey', 'White',
      'Red 1', 'Yellow 1',    'Green 1',     'Blue 1',     'Tan 1',
      'Red 2', 'Yellow 2',    'Green 2',     'Blue 2',     'Tan 2',
      'Red 3', 'Yellow 3',    'Green 3',     'Blue 3',     'Tan 3',
    ];
    chrome.test.assertEq(expectedLabels.length, colorButtons.length);

    for (let i = 0; i < colorButtons.length; ++i) {
      assertLabels(colorButtons[i]!, expectedLabels[i]!);
    }

    chrome.test.succeed();
  },

  // TODO(crbug.com/440740223): Deflake and re-enable.
  // async function testFocusesSelectedItem() {
  //   let selector = createSelector(hexToColor(PEN_COLORS[1]!.color));
  //   let colorButtons = getColorButtons(selector);
  //   chrome.test.assertEq(20, colorButtons.length);
  //   assertSelectedColor(colorButtons, /*buttonIndex=*/ 1);
  //   let whenFocused = eventToPromise('focus', colorButtons[1]!);
  //   selector.focus();
  //   await whenFocused;

  //   // Recreate the selector to test a different initial condition.
  //   selector = createSelector(hexToColor(PEN_COLORS[10]!.color));
  //   colorButtons = getColorButtons(selector);
  //   chrome.test.assertEq(20, colorButtons.length);
  //   assertSelectedColor(colorButtons, /*buttonIndex=*/ 10);
  //   whenFocused = eventToPromise('focus', colorButtons[10]!);
  //   selector.focus();
  //   await whenFocused;
  //   chrome.test.succeed();
  // },
]);
