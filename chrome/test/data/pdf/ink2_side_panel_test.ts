// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {AnnotationBrush} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
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

    // Change the pen size.
    const sizeButtons = getSizeButtons();
    sizeButtons[0].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });

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

    // Change the eraser size.
    const sizeButtons = getSizeButtons();
    sizeButtons[1].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 2,
    });

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

    // Change the highlighter size.
    const sizeButtons = getSizeButtons();
    sizeButtons[4].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 16,
    });

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

    // Switch back to eraser. It should have the previous size.
    sidePanel.$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 2,
    });

    // Switch back to highlighter. It should have the previous color and size.
    sidePanel.$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 52, g: 168, b: 83},
      size: 16,
    });
    chrome.test.succeed();
  },
]);
