// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkSizeSelectorElement, ViewerBottomToolbarDropdownElement, ViewerBottomToolbarElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, assertSelectedSize, getBrushSelector, getRequiredElement, getSizeButtons, setGetAnnotationBrushReply, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();

// Enter annotation mode to get the side panel.
viewer.$.toolbar.toggleAnnotation();
await microtasksFinished();
assert(viewer.$.toolbar.annotationMode);

const bottomToolbar = getRequiredElement<ViewerBottomToolbarElement>(
    viewer, 'viewer-bottom-toolbar');

async function clickDropdownButton(
    dropdown: ViewerBottomToolbarDropdownElement) {
  const dropdownButton = getRequiredElement(dropdown, 'cr-icon-button');
  dropdownButton.click();
  await microtasksFinished();
}

chrome.test.runTests([
  async function testSelectPen() {
    // Default to a black pen. Cannot use assertAnnotationBrush() yet, since
    // there's no need to set the brush in the backend immediately after getting
    // the default brush.

    // Change the pen size.
    await clickDropdownButton(bottomToolbar.$.size);
    const sizeSelector = getRequiredElement<InkSizeSelectorElement>(
        bottomToolbar, 'ink-size-selector');
    const sizeButtons = getSizeButtons(sizeSelector);
    assertSelectedSize(sizeButtons, /*buttonIndex=*/ 2);

    sizeButtons[0].click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });
    chrome.test.succeed();
  },

  // Test that the eraser can be selected.
  async function testSelectEraser() {
    // Switch to eraser.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.ERASER, /*size=*/ 3);
    getBrushSelector(bottomToolbar).$.eraser.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.ERASER,
      size: 3,
    });

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
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    // Switch to highlighter.
    setGetAnnotationBrushReply(
        mockPlugin, AnnotationBrushType.HIGHLIGHTER, /*size=*/ 8,
        /*color=*/ {r: 242, g: 139, b: 130});
    getBrushSelector(bottomToolbar).$.highlighter.click();
    await microtasksFinished();

    assertAnnotationBrush(mockPlugin, {
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 242, g: 139, b: 130},
      size: 8,
    });

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
    chrome.test.succeed();
  },
]);
