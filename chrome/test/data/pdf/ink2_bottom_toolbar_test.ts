// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertAnnotationBrush, getBrushSelector, getRequiredElement, setGetAnnotationBrushReply, setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const mockPlugin = setupTestMockPluginForInk();

// Enter annotation mode to get the side panel.
viewer.$.toolbar.toggleAnnotation();
await microtasksFinished();
assert(viewer.$.toolbar.annotationMode);

const bottomToolbar =
    getRequiredElement<HTMLElement>(viewer, 'viewer-bottom-toolbar');

chrome.test.runTests([
  // TODO(crbug.com/369653190): Test the default pen once a setAnnotationBrush
  // message is triggered from selecting a size or color.

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
    chrome.test.succeed();
  },
]);
