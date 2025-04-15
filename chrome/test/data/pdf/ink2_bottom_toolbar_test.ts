// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, AnnotationMode, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkColorSelectorElement, InkSizeSelectorElement, ViewerBottomToolbarDropdownElement, ViewerBottomToolbarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertSelectedSize, clickDropdownButton, getBrushSelector, getColorButtons, getRequiredElement, getSizeButtons, setGetAnnotationBrushReply, setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

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
  const button = colorButtons[index];
  chrome.test.assertTrue(!!button);
  button.click();
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
      getComputedStyle(getRequiredElement(bottomToolbar, '.color-chip'));
  chrome.test.assertEq(expected, styles.getPropertyValue('background-color'));
}

chrome.test.runTests([
  // Test that toggling annotation mode opens the bottom toolbar when text
  // annotation is enabled.
  async function testOpenBottomToolbarTextEnabled() {
    mockMetricsPrivate.reset();

    // Enable text annotations.
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': true});
    viewer.$.toolbar.strings = Object.assign({}, viewer.$.toolbar.strings);
    await microtasksFinished();

    // No toolbars initially.
    const drawToolbarQuery = 'viewer-bottom-toolbar';
    const textToolbarQuery = 'viewer-text-bottom-toolbar';
    chrome.test.assertEq(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(drawToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(textToolbarQuery));

    // Text annotation mode shows the text toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewer.$.toolbar.annotationMode);
    const drawToolbar = viewer.shadowRoot.querySelector(drawToolbarQuery);
    const textToolbar = viewer.shadowRoot.querySelector(textToolbarQuery);
    chrome.test.assertTrue(isVisible(textToolbar));
    chrome.test.assertFalse(isVisible(drawToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // Draw annotation mode shows the drawing toolbar.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(isVisible(textToolbar));
    chrome.test.assertTrue(isVisible(drawToolbar));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    // Still 1, because we're still using the bottom toolbar, just in a
    // different mode.
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);

    // No annotation mode removes both toolbars from the DOM.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(drawToolbarQuery));
    chrome.test.assertFalse(
        !!viewer.shadowRoot.querySelector(textToolbarQuery));
    chrome.test.succeed();
  },

  // Test that toggling annotation mode opens the bottom toolbar. Must be run
  // before remaining tests, as other tests expect to already be in annotation
  // mode.
  async function testOpenBottomToolbar() {
    mockMetricsPrivate.reset();

    // Disable text annotations.
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': false});
    viewer.$.toolbar.strings = Object.assign({}, viewer.$.toolbar.strings);
    await microtasksFinished();

    viewer.$.toolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();

    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);
    chrome.test.assertTrue(
        !!viewer.shadowRoot.querySelector('viewer-bottom-toolbar'));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 0);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 1);
    chrome.test.succeed();
  },

  async function testSelectPen() {
    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);

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
    assertDropdownSizeIcon('pdf-ink:pen-size-3');
    sizeButtons[0]!.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });
    assertDropdownSizeIcon('pdf-ink:pen-size-1');

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
    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);

    // Switch to eraser.
    setGetAnnotationBrushReply(mockPlugin, AnnotationBrushType.ERASER);
    const bottomToolbar = getBottomToolbar();
    getBrushSelector(bottomToolbar).$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.ERASER,
    });

    // There shouldn't be color options.
    chrome.test.assertTrue(
        !bottomToolbar.shadowRoot.querySelector<HTMLElement>('#color'));
    // There shouldn't be size options.
    chrome.test.assertTrue(
        !bottomToolbar.shadowRoot.querySelector<HTMLElement>('#size'));
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    chrome.test.assertEq(AnnotationMode.DRAW, viewer.$.toolbar.annotationMode);

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
    assertDropdownSizeIcon('pdf-ink:highlighter-size-3');

    // Change the highlighter size.
    await clickDropdownButton(bottomToolbar.$.size);
    const sizeSelector = getRequiredElement<InkSizeSelectorElement>(
        bottomToolbar, 'ink-size-selector');
    const sizeButtons = getSizeButtons(sizeSelector);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the highlighter size.
    sizeButtons[4]!.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 16,
    });
    assertDropdownSizeIcon('pdf-ink:highlighter-size-5');

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
