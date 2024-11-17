// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkColorSelectorElement, InkSizeSelectorElement, ViewerBottomToolbarDropdownElement, ViewerBottomToolbarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertSelectedSize, getBrushSelector, getColorButtons, getRequiredElement, getSizeButtons, setGetAnnotationBrushReply, setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();

function getBottomToolbar(): ViewerBottomToolbarElement {
  return getRequiredElement(viewer, 'viewer-bottom-toolbar');
}

async function clickColorButton(index: number) {
  const bottomToolbar = getBottomToolbar();
  const colorDropdown = getRequiredElement<ViewerBottomToolbarDropdownElement>(
      bottomToolbar, '#color');
  await clickDropdownButton(colorDropdown);

  const colorSelector = getRequiredElement<InkColorSelectorElement>(
      bottomToolbar, 'ink-color-selector');
  const colorButtons = getColorButtons(colorSelector);
  colorButtons[index].click();
  await microtasksFinished();
}

async function clickDropdownButton(
    dropdown: ViewerBottomToolbarDropdownElement) {
  const dropdownButton = getRequiredElement(dropdown, 'cr-button');
  dropdownButton.click();
  await microtasksFinished();
}

function assertDropdownSizeIcon(expected: string) {
  const bottomToolbar = getBottomToolbar();
  const actual =
      getRequiredElement(bottomToolbar, '#size > cr-icon').getAttribute('icon');
  chrome.test.assertTrue(!!actual);
  chrome.test.assertEq(expected, actual);
}

function assertDropdownColorFillColor(expected: string) {
  const bottomToolbar = getBottomToolbar();
  const styles =
      getComputedStyle(getRequiredElement(bottomToolbar, '#color-chip'));
  chrome.test.assertEq(expected, styles.getPropertyValue('background-color'));
}

chrome.test.runTests([
  // Test that toggling annotation mode opens the bottom toolbar. Must be run
  // first, as other tests expect to already be in annotation mode.
  async function testOpenBottomToolbar() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    viewer.$.toolbar.toggleAnnotation();
    await microtasksFinished();

    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);
    chrome.test.assertTrue(
        !!viewer.shadowRoot!.querySelector('viewer-bottom-toolbar'));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);
    chrome.test.succeed();
  },

  async function testSelectPen() {
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    // Default to a black pen. Cannot use assertAnnotationBrush() yet, since
    // there's no need to set the brush in the backend immediately after getting
    // the default brush.

    // Change the pen size.
    const bottomToolbar = getBottomToolbar();
    await clickDropdownButton(bottomToolbar.$.size);
    const sizeSelector = getRequiredElement<InkSizeSelectorElement>(
        bottomToolbar, 'ink-size-selector');
    const sizeButtons = getSizeButtons(sizeSelector);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);
    assertDropdownSizeIcon('pdf:pen-size-3');
    sizeButtons[0].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });
    assertDropdownSizeIcon('pdf:pen-size-1');

    // Change the pen color to '#fdd663'.
    await clickColorButton(/*index=*/ 6);

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.PEN,
      color: {r: 253, g: 214, b: 99},
      size: 1,
    });
    assertDropdownColorFillColor('rgb(253, 214, 99)');
    chrome.test.succeed();
  },

  // Test that the eraser can be selected.
  async function testSelectEraser() {
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    // Switch to eraser.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.ERASER, /*size=*/ 3);
    const bottomToolbar = getBottomToolbar();
    getBrushSelector(bottomToolbar).$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.ERASER,
      size: 3,
    });
    assertDropdownSizeIcon('pdf:eraser-size-3');

    // Change the eraser size.
    await clickDropdownButton(bottomToolbar.$.size);
    const sizeSelector = getRequiredElement<InkSizeSelectorElement>(
        bottomToolbar, 'ink-size-selector');
    const sizeButtons = getSizeButtons(sizeSelector);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    sizeButtons[1].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.ERASER,
      size: 2,
    });
    assertDropdownSizeIcon('pdf:eraser-size-2');

    // There shouldn't be color options.
    chrome.test.assertTrue(
        !bottomToolbar.shadowRoot!.querySelector<HTMLElement>('#color'));
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    // Switch to highlighter.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.HIGHLIGHTER, /*size=*/ 8,
        /*color=*/ {r: 242, g: 139, b: 130});
    const bottomToolbar = getBottomToolbar();
    getBrushSelector(bottomToolbar).$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 8,
    });
    assertDropdownSizeIcon('pdf:highlighter-size-3');

    // Change the highlighter size.
    await clickDropdownButton(bottomToolbar.$.size);
    const sizeSelector = getRequiredElement<InkSizeSelectorElement>(
        bottomToolbar, 'ink-size-selector');
    const sizeButtons = getSizeButtons(sizeSelector);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the highlighter size.
    sizeButtons[4].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 16,
    });
    assertDropdownSizeIcon('pdf:highlighter-size-5');

    // Change the highlighter color to '#34a853'.
    await clickColorButton(/*index=*/ 2);

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 52, g: 168, b: 83},
      size: 16,
    });
    // The color changes to 'rgb(174, 220, 186)' after blending.
    assertDropdownColorFillColor('rgb(174, 220, 186)');
    chrome.test.succeed();
  },
]);
