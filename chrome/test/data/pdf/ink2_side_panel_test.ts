// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {AnnotationBrush} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createMockPdfPluginForTest} from './test_util.js';

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
  const eraserIcon = sidePanel.$.eraser.getAttribute('iron-icon');
  assert(eraserIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ? 'pdf:ink-eraser-fill' :
                                                         'pdf:ink-eraser',
      eraserIcon);

  const highlighterIcon = sidePanel.$.highlighter.getAttribute('iron-icon');
  assert(highlighterIcon);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ?
          'pdf:ink-highlighter-fill' :
          'pdf:ink-highlighter',
      highlighterIcon);

  const penIcon = sidePanel.$.pen.getAttribute('iron-icon');
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
  const eraserSelected = sidePanel.$.eraser.dataset['selected'];
  assert(eraserSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.ERASER ? 'true' : 'false',
      eraserSelected);

  const highlighterSelected = sidePanel.$.highlighter.dataset['selected'];
  assert(highlighterSelected);
  chrome.test.assertEq(
      selectedBrushType === AnnotationBrushType.HIGHLIGHTER ? 'true' : 'false',
      highlighterSelected);

  const penSelected = sidePanel.$.pen.dataset['selected'];
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
 * Helper to get a non-empty list of brush size buttons.
 * @returns A list of exactly 5 size buttons.
 */
function getSizeButtons(): NodeListOf<HTMLElement> {
  const sizeButtons = sidePanel.shadowRoot!.querySelectorAll<HTMLElement>(
      '#sizes cr-icon-button');
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
    sidePanel.$.eraser.click();
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
    sidePanel.$.highlighter.click();
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
    sidePanel.$.pen.click();
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
    sidePanel.$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 2,
    });
    assertBrushIcons(AnnotationBrushType.ERASER);
    assertSelectedBrush(AnnotationBrushType.ERASER);
    assertSelectedSize(/*buttonIndex=*/ 1);

    // Switch back to highlighter. It should have the previous color and size.
    sidePanel.$.highlighter.click();
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
]);
