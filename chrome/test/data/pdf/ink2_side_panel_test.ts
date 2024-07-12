// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {AnnotationBrush} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
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

chrome.test.runTests([
  // Test that the pen can be selected. Test that its size and color can be
  // selected.
  async function testSelectPen() {
    // Default to pen.
    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });
    chrome.test.succeed();
  },

  // Test that the eraser can be selected.
  async function testSelectEraser() {
    // Switch to eraser.
    sidePanel.$.eraser.click();
    await microtasksFinished();

    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });
    chrome.test.succeed();
  },

  // Test that the highlighter can be selected.
  async function testSelectHighlighter() {
    // Switch to highlighter.
    sidePanel.$.highlighter.click();
    await microtasksFinished();

    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });
    chrome.test.succeed();
  },

  // Test that when brushes are changed again, the selected brush should have
  // the same settings as last set in previous tests.
  async function testGoBackToBrushWithPreviousSettings() {
    // Switch back to pen. It should have the previous color and size.
    sidePanel.$.pen.click();
    await microtasksFinished();

    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });

    // Switch back to eraser. It should have the previous size.
    sidePanel.$.eraser.click();
    await microtasksFinished();

    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    assertAnnotationBrush({
      type: AnnotationBrushType.ERASER,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });

    // Switch back to highlighter. It should have the previous color and size.
    sidePanel.$.highlighter.click();
    await microtasksFinished();

    // TODO(crbug.com/351868764): Set actual values for the colors and size.
    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });
    chrome.test.succeed();
  },
]);
