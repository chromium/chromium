// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkColorSelectorElement, InkSizeSelectorElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertSelectedSize, getBrushSelector, getColorButtons, getRequiredElement, getSizeButtons, setGetAnnotationBrushReply, setupMockMetricsPrivate, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();

function getSidePanel(): HTMLElement {
  return getRequiredElement(viewer, 'viewer-side-panel');
}

function getSizeSelector(): InkSizeSelectorElement {
  return getRequiredElement<InkSizeSelectorElement>(
      getSidePanel(), 'ink-size-selector');
}

function getColorSelector(): InkColorSelectorElement {
  return getRequiredElement<InkColorSelectorElement>(
      getSidePanel(), 'ink-color-selector');
}

chrome.test.runTests([
  // Test that toggling annotation mode opens the side panel. Must be run first,
  // as other tests expect to already be in annotation mode.
  async function testOpenSidePanel() {
    const mockMetricsPrivate = setupMockMetricsPrivate();

    viewer.$.toolbar.toggleAnnotation();
    await microtasksFinished();

    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);
    chrome.test.assertTrue(
        !!viewer.shadowRoot!.querySelector('viewer-side-panel'));
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_SIDE_PANEL, 1);
    mockMetricsPrivate.assertCount(UserAction.OPEN_INK2_BOTTOM_TOOLBAR, 0);
    chrome.test.succeed();
  },

  // Test that the pen can be selected. Test that its size and color can be
  // selected.
  async function testSelectPen() {
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    // Default to a black pen. Cannot use assertAnnotationBrush() yet, since
    // there's no need to set the brush in the backend immediately after getting
    // the default brush.
    const sizeButtons = getSizeButtons(getSizeSelector());
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the pen size.
    sizeButtons[0].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });

    // Change the pen color to '#fdd663'.
    const colorButtons = getColorButtons(getColorSelector());
    colorButtons[6].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.PEN,
      color: {r: 253, g: 214, b: 99},
      size: 1,
    });
    chrome.test.succeed();
  },

  // Test that the eraser can be selected.
  async function testSelectEraser() {
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    // Switch to eraser.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.ERASER, /*size=*/ 3);
    getBrushSelector(getSidePanel()).$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.ERASER,
      size: 3,
    });

    const sizeButtons = getSizeButtons(getSizeSelector());
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the eraser size.
    sizeButtons[1].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.ERASER,
      size: 2,
    });

    // There shouldn't be color options.
    const sidePanel = getSidePanel();
    chrome.test.assertTrue(!sidePanel.shadowRoot!.querySelector<HTMLElement>(
        'ink-color-selector'));
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    chrome.test.assertTrue(viewer.$.toolbar.annotationMode);

    // Switch to highlighter.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.HIGHLIGHTER, /*size=*/ 8,
        /*color=*/ {r: 242, g: 139, b: 130});
    getBrushSelector(getSidePanel()).$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 8,
    });

    const sizeButtons = getSizeButtons(getSizeSelector());
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    // Change the highlighter size.
    sizeButtons[4].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 16,
    });

    // Change the highlighter color to '#34a853'.
    const colorButtons = getColorButtons(getColorSelector());
    colorButtons[2].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 52, g: 168, b: 83},
      size: 16,
    });
    chrome.test.succeed();
  },
]);
