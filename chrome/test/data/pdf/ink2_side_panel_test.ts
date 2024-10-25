// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {AnnotationBrush, InkBrushSelectorElement, InkColorSelectorElement, InkSizeSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertSelectedSize, getColorButtons, getRequiredElement, getSizeButtons, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();

// Enter annotation mode to get the side panel.
viewer.$.toolbar.toggleAnnotation();
await microtasksFinished();
assert(viewer.$.toolbar.annotationMode);

const sidePanel = getRequiredElement<HTMLElement>(viewer, 'viewer-side-panel');

function setGetAnnotationBrushReply(
    type: AnnotationBrushType, size: number,
    color?: {r: number, g: number, b: number}) {
  mockPlugin.setMessageReply('getAnnotationBrush', {data: {type, size, color}});
}

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
 * @returns The non-null brush type selector.
 */
function getBrushSelector(): InkBrushSelectorElement {
  return getRequiredElement<InkBrushSelectorElement>(
      sidePanel, 'ink-brush-selector');
}

function getSizeSelector(): InkSizeSelectorElement {
  return getRequiredElement<InkSizeSelectorElement>(
      sidePanel, 'ink-size-selector');
}

function getColorSelector(): InkColorSelectorElement {
  return getRequiredElement<InkColorSelectorElement>(
      sidePanel, 'ink-color-selector');
}

chrome.test.runTests([
  // Test that the pen can be selected. Test that its size and color can be
  // selected.
  async function testSelectPen() {
    // Default to a black pen. Cannot use assertAnnotationBrush() yet, since
    // there's no need to set the brush in the backend immediately after getting
    // the default brush.
    const sizeButtons = getSizeButtons(getSizeSelector());
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the pen size.
    sizeButtons[0].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });

    // Change the pen color to '#fdd663'.
    const colorButtons = getColorButtons(getColorSelector());
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
    setGetAnnotationBrushReply(AnnotationBrushType.ERASER, /*size=*/ 3);
    getBrushSelector().$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 3,
    });

    const sizeButtons = getSizeButtons(getSizeSelector());
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the eraser size.
    sizeButtons[1].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      size: 2,
    });

    // There shouldn't be color options.
    chrome.test.assertTrue(!sidePanel.shadowRoot!.querySelector<HTMLElement>(
        'ink-color-selector'));
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    // Switch to highlighter.
    setGetAnnotationBrushReply(
        AnnotationBrushType.HIGHLIGHTER, /*size=*/ 8,
        /*color=*/ {r: 242, g: 139, b: 130});
    getBrushSelector().$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 8,
    });

    const sizeButtons = getSizeButtons(getSizeSelector());
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the highlighter size.
    sizeButtons[4].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 16,
    });

    // Change the highlighter color to '#34a853'.
    const colorButtons = getColorButtons(getColorSelector());
    colorButtons[2].click();
    await microtasksFinished();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 52, g: 168, b: 83},
      size: 16,
    });
    chrome.test.succeed();
  },
]);
