// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {AnnotationBrush, InkBrushSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createMockPdfPluginForTest, getRequiredElement} from './test_util.js';

const controller = PluginController.getInstance();
const mockPlugin = createMockPdfPluginForTest();
controller.setPluginForTesting(mockPlugin);

// Create a standalone side panel and use it for all tests.
const sidePanel = document.createElement('viewer-side-panel');
document.body.innerHTML = '';
document.body.appendChild(sidePanel);

/**
 * Tests that the current annotation brush matches `expectedBrush`. Clears all
 * messages from `mockPlugin` after, otherwise subsequent calls would continue
 * to find and use the same message.
 * @param expectedBrush The expected brush that the current annotation brush
 * should match.
 */
function assertAnnotationBrush(expectedBrush: AnnotationBrush) {
  const setAnnotationBrushMessage =
      mockPlugin.findMessage('setAnnotationBrush');
  chrome.test.assertTrue(setAnnotationBrushMessage !== undefined);
  chrome.test.assertEq('setAnnotationBrush', setAnnotationBrushMessage.type);
  chrome.test.assertEq(expectedBrush.type, setAnnotationBrushMessage.data.type);
  const hasColor = expectedBrush.color !== undefined;
  chrome.test.assertEq(
      hasColor, setAnnotationBrushMessage.data.color !== undefined);
  if (hasColor) {
    chrome.test.assertEq(
        expectedBrush.color!.r, setAnnotationBrushMessage.data.color.r);
    chrome.test.assertEq(
        expectedBrush.color!.g, setAnnotationBrushMessage.data.color.g);
    chrome.test.assertEq(
        expectedBrush.color!.b, setAnnotationBrushMessage.data.color.b);
  }
  chrome.test.assertEq(expectedBrush.size, setAnnotationBrushMessage.data.size);

  mockPlugin.clearMessages();
}

/**
 * Tests that the correct brush icons are displayed, depending on what brush
 * is selected. The brush type matching `selectedBrushType` should have a filled
 * icon.
 * @param selectedBrushType The expected selected brush type that should
 * have a filled icon.
 */
function assertBrushIcons(selectedBrushType: AnnotationBrushType) {
  const eraserIcon = getBrushSelector().$.eraser.getAttribute('iron-icon');
  assert(eraserIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ? 'pdf:ink-eraser-fill' :
                                                         'pdf:ink-eraser',
      eraserIcon);

  const highlighterIcon =
      getBrushSelector().$.highlighter.getAttribute('iron-icon');
  assert(highlighterIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ?
          'pdf:ink-highlighter-fill' :
          'pdf:ink-highlighter',
      highlighterIcon);

  const penIcon = getBrushSelector().$.pen.getAttribute('iron-icon');
  assert(penIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.PEN ? 'pdf:ink-pen-fill' :
                                                      'pdf:ink-pen',
      penIcon);
}

/**
 * Tests that the brushes have correct values for the selected attribute. The
 * brush type matching `selectedBrushType` should be selected.
 * @param selectedBrushType The expected selected brush type.
 */
function assertSelectedBrush(selectedBrushType: AnnotationBrushType) {
  const eraserSelected = getBrushSelector().$.eraser.dataset['selected'];
  assert(eraserSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ? 'true' : 'false',
      eraserSelected);

  const highlighterSelected =
      getBrushSelector().$.highlighter.dataset['selected'];
  assert(highlighterSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ? 'true' : 'false',
      highlighterSelected);

  const penSelected = getBrushSelector().$.pen.dataset['selected'];
  assert(penSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.PEN ? 'true' : 'false',
      penSelected);
}

/**
 * Tests that the size options have corrected values for the selected attribute.
 * The size button with index `buttonIndex` should be selected.
 * @param buttonIndex The expected selected size button.
 */
function assertSelectedSize(buttonIndex: number) {
  const sizeButtons = getSizeButtons();
  for (let i = 0; i < sizeButtons.length; ++i) {
    const buttonSelected = sizeButtons[i].dataset['selected'];
    chrome.test.assertEq(i === buttonIndex ? 'true' : 'false', buttonSelected);
  }
}

/**
 * Tests that the color options have corrected values for the selected
 * attribute. The color button with index `buttonIndex` should be selected.
 * @param buttonIndex The expected selected color button.
 */
function assertSelectedColor(buttonIndex: number) {
  const colorButtons = getColorButtons();
  for (let i = 0; i < colorButtons.length; ++i) {
    chrome.test.assertEq(
        i === buttonIndex, colorButtons[i].hasAttribute('checked'));
  }
}

/**
 * @returns The non-null brush type selector.
 */
function getBrushSelector(): InkBrushSelectorElement {
  return getRequiredElement<InkBrushSelectorElement>(
      sidePanel, 'ink-brush-selector');
}

/**
 * Helper to get a non-empty list of brush size buttons.
 * @returns A list of exactly 5 size buttons.
 */
function getSizeButtons(): NodeListOf<HTMLElement> {
  const sizeSelector =
      getRequiredElement<HTMLElement>(sidePanel, 'ink-size-selector');
  const sizeButtons =
      sizeSelector.shadowRoot!.querySelectorAll<HTMLElement>('cr-icon-button');
  assert(sizeButtons);
  assert(sizeButtons.length === 5);
  return sizeButtons;
}

/**
 * Helper to get a non-null list of brush color buttons. Can be empty.
 * @returns A list of color buttons.
 */
function getColorButtons(): NodeListOf<HTMLElement> {
  const colorButtons =
      sidePanel.shadowRoot!.querySelectorAll<HTMLElement>('#colors input');
  assert(colorButtons);
  return colorButtons;
}

/**
 * Tests that the selected size index is `expectedButtonIndex` after
 * `targetElement` receives a keyboard event with key `key`.
 * @param targetElement The target element to receive the keyboard event.
 * @param key The key being pressed.
 * @param expectedButtonIndex The expected size button index after the keyboard
 *     event.
 */
async function testSizeKeyboardEvent(
    targetElement: HTMLElement, key: string, expectedButtonIndex: number) {
  keyDownOn(targetElement, 0, [], key);
  await microtasksFinished();

  assertSelectedSize(/*buttonIndex=*/ expectedButtonIndex);
}

/**
 * Tests that the selected color index is `expectedButtonIndex` after
 * `targetElement` receives a keyboard event with key `key`.
 * @param targetElement The target element to receive the keyboard event.
 * @param key The key being pressed.
 * @param expectedButtonIndex The expected color button index after the keyboard
 *     event.
 */
async function testColorKeyboardEvent(
    targetElement: HTMLElement, key: string, expectedButtonIndex: number) {
  keyDownOn(targetElement, 0, [], key);
  await microtasksFinished();

  assertSelectedColor(/*buttonIndex=*/ expectedButtonIndex);
}

chrome.test.runTests([
  // Test that the pen can be selected. Test that its size and color can be
  // selected.
  async function testSelectPen() {
    // Default to a black pen.
    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 3,
    });
    assertBrushIcons(AnnotationBrushType.PEN);
    assertSelectedBrush(AnnotationBrushType.PEN);
    assertSelectedSize(/*buttonIndex=*/ 2);

    // Change the pen size.
    const sizeButtons = getSizeButtons();
    sizeButtons[0].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });
    assertSelectedSize(/*buttonIndex=*/ 0);

    // Change the pen color.
    // Pens should have 20 color options.
    const colorButtons = getColorButtons();
    chrome.test.assertEq(20, colorButtons.length);

    // Click the color corresponding to '#fdd663'.
    colorButtons[6].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 253, g: 214, b: 99},
      size: 1,
    });
    chrome.test.succeed();
  },

  // Test that the eraser can be selected.
  async function testSelectEraser() {
    // Switch to eraser.
    getBrushSelector().$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 3,
    });
    assertBrushIcons(AnnotationBrushType.ERASER);
    assertSelectedBrush(AnnotationBrushType.ERASER);
    assertSelectedSize(/*buttonIndex=*/ 2);

    // Change the eraser size.
    const sizeButtons = getSizeButtons();
    sizeButtons[1].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 2,
    });
    assertSelectedSize(/*buttonIndex=*/ 1);

    // There shouldn't be any color buttons.
    const colorButtons = getColorButtons();
    chrome.test.assertTrue(!colorButtons.length);
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    // Switch to highlighter.
    getBrushSelector().$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 8,
    });
    assertBrushIcons(AnnotationBrushType.HIGHLIGHTER);
    assertSelectedBrush(AnnotationBrushType.HIGHLIGHTER);
    assertSelectedSize(/*buttonIndex=*/ 2);

    // Change the highlighter size.
    const sizeButtons = getSizeButtons();
    sizeButtons[4].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 16,
    });
    assertSelectedSize(/*buttonIndex=*/ 4);

    // Change the highlighter color.
    // Highlighters should have 10 color options.
    const colorButtons = getColorButtons();
    chrome.test.assertEq(10, colorButtons.length);

    // Click the color corresponding to '#34a853'.
    colorButtons[2].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 52, g: 168, b: 83},
      size: 16,
    });
    chrome.test.succeed();
  },

  // Test that when brushes are changed again, the selected brush should have
  // the same settings as last set in previous tests.
  async function testGoBackToBrushWithPreviousSettings() {
    // Switch back to pen. It should have the previous color and size.
    getBrushSelector().$.pen.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 253, g: 214, b: 99},
      size: 1,
    });
    assertBrushIcons(AnnotationBrushType.PEN);
    assertSelectedBrush(AnnotationBrushType.PEN);
    assertSelectedSize(/*buttonIndex=*/ 0);

    // Switch back to eraser. It should have the previous size.
    getBrushSelector().$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 2,
    });
    assertBrushIcons(AnnotationBrushType.ERASER);
    assertSelectedBrush(AnnotationBrushType.ERASER);
    assertSelectedSize(/*buttonIndex=*/ 1);

    // Switch back to highlighter. It should have the previous color and size.
    getBrushSelector().$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 52, g: 168, b: 83},
      size: 16,
    });
    assertBrushIcons(AnnotationBrushType.HIGHLIGHTER);
    assertSelectedBrush(AnnotationBrushType.HIGHLIGHTER);
    assertSelectedSize(/*buttonIndex=*/ 4);
    chrome.test.succeed();
  },

  // Test that arrow keys does nothing to the size when the size buttons are not
  // focused.
  async function testArrowKeysSizeNonFocused() {
    assertSelectedSize(/*buttonIndex=*/ 4);

    // Press arrow keys on the root element. This should not change the size.
    await testSizeKeyboardEvent(
        document.documentElement, 'ArrowLeft', /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        document.documentElement, 'ArrowUp', /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        document.documentElement, 'ArrowRight', /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        document.documentElement, 'ArrowDown', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test that arrow keys change brush sizes when the brush size buttons are
  // focused.
  async function testArrowKeysChangeSize() {
    assertSelectedSize(/*buttonIndex=*/ 4);

    const sizeButtons = getSizeButtons();

    // Pressing 'ArrowLeft' or 'ArrowUp' should select the previous size button.
    await testSizeKeyboardEvent(
        sizeButtons[4], 'ArrowLeft', /*expectedButtonIndex=*/ 3);

    await testSizeKeyboardEvent(
        sizeButtons[3], 'ArrowUp', /*expectedButtonIndex=*/ 2);

    // Pressing 'ArrowRight' or 'ArrowDown' should select the next size button.
    await testSizeKeyboardEvent(
        sizeButtons[2], 'ArrowRight', /*expectedButtonIndex=*/ 3);

    await testSizeKeyboardEvent(
        sizeButtons[3], 'ArrowDown', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test that when the last size button is selected, pressing 'ArrowRight' or
  // 'ArrowDown' will select the first size button.
  // Test that when the first size button is selected, pressing 'ArrowLeft' or
  // 'ArrowUp' will select the last size button.
  async function testArrowKeysChangeSizeFirstLast() {
    assertSelectedSize(/*buttonIndex=*/ 4);

    const sizeButtons = getSizeButtons();

    await testSizeKeyboardEvent(
        sizeButtons[4], 'ArrowRight', /*expectedButtonIndex=*/ 0);

    await testSizeKeyboardEvent(
        sizeButtons[0], 'ArrowLeft', /*expectedButtonIndex=*/ 4);

    await testSizeKeyboardEvent(
        sizeButtons[4], 'ArrowDown', /*expectedButtonIndex=*/ 0);

    await testSizeKeyboardEvent(
        sizeButtons[0], 'ArrowUp', /*expectedButtonIndex=*/ 4);

    chrome.test.succeed();
  },

  // Test that arrow keys does nothing to the color when the color buttons are
  // not focused.
  async function testArrowKeysColorNonFocused() {
    assertSelectedColor(/*buttonIndex=*/ 2);

    // Press arrow keys on the root element. This should not change the size.
    await testColorKeyboardEvent(
        document.documentElement, 'ArrowLeft', /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        document.documentElement, 'ArrowRight', /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        document.documentElement, 'ArrowUp', /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        document.documentElement, 'ArrowDown', /*expectedButtonIndex=*/ 2);

    chrome.test.succeed();
  },

  // Test that left and right arrow keys change brush colors when the brush
  // color buttons are focused.
  async function testArrowKeysChangeColor() {
    assertSelectedColor(/*buttonIndex=*/ 2);

    const colorButtons = getColorButtons();

    await testColorKeyboardEvent(
        colorButtons[2], 'ArrowLeft', /*expectedButtonIndex=*/ 1);

    await testColorKeyboardEvent(
        colorButtons[1], 'ArrowRight', /*expectedButtonIndex=*/ 2);

    await testColorKeyboardEvent(
        colorButtons[2], 'ArrowDown', /*expectedButtonIndex=*/ 7);

    await testColorKeyboardEvent(
        colorButtons[7], 'ArrowUp', /*expectedButtonIndex=*/ 2);

    chrome.test.succeed();
  },

  // Test that when the color button in the first column is selected, pressing
  // 'ArrowLeft' will select the color button in the last column in the same
  // row.
  // Test that when the color button in the last column is selected,
  // pressing 'ArrowRight' will select the color button in the first column in
  // the same row.
  async function testArrowKeysChangeColorFirstLastColumn() {
    const colorButtons = getColorButtons();

    getColorButtons()[5].click();
    await microtasksFinished();

    assertSelectedColor(5);

    await testColorKeyboardEvent(
        colorButtons[5], 'ArrowLeft', /*expectedButtonIndex=*/ 9);

    await testColorKeyboardEvent(
        colorButtons[9], 'ArrowRight', /*expectedButtonIndex=*/ 5);

    chrome.test.succeed();
  },

  // Test that when the color button in the first row is selected, pressing
  // 'ArrowUp' will select the color button in the last row in the same column.
  // Test that when the color button in the last row is selected,
  // pressing 'ArrowDown' will select the color button in the first row in the
  // same column.
  async function testArrowKeysChangeColorFirstLastRow() {
    // Switch to pen, which has multiple rows of colors.
    getBrushSelector().$.pen.click();
    await microtasksFinished();

    const colorButtons = getColorButtons();

    colorButtons[1].click();
    await microtasksFinished();

    assertSelectedColor(1);

    await testColorKeyboardEvent(
        colorButtons[1], 'ArrowUp', /*expectedButtonIndex=*/ 16);

    await testColorKeyboardEvent(
        colorButtons[16], 'ArrowDown', /*expectedButtonIndex=*/ 1);

    chrome.test.succeed();
  },
]);
