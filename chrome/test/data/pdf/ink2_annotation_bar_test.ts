// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationBrushType, PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {AnnotationBrush} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createMockPdfPluginForTest, finishInkStroke, getAnnotationsBar} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const controller = PluginController.getInstance();
const mockPlugin = createMockPdfPluginForTest();
controller.setPluginForTesting(mockPlugin);

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
  // Test that the annotations bar is shown when annotation mode is enabled and
  // hidden when annotation mode is disabled.
  async function testAnnotationBar() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    await waitAfterNextRender(viewerToolbar);
    const annotationsBar = getAnnotationsBar(viewerToolbar);

    // Annotations bar should be visible when annotation mode is enabled.
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    chrome.test.assertTrue(isVisible(annotationsBar));

    viewerToolbar.toggleAnnotation();
    await waitAfterNextRender(viewerToolbar);

    // Annotations bar should be hidden when annotation mode is disabled.
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    chrome.test.assertFalse(isVisible(annotationsBar));
    chrome.test.succeed();
  },
  // Test that the pen can be selected. Test that its size and color can be
  // selected.
  async function testSelectPen() {
    viewerToolbar.toggleAnnotation();
    await waitAfterNextRender(viewerToolbar);
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    const annotationBar = getAnnotationsBar(viewerToolbar);

    // Default to pen.
    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 0.1429,
    });

    // Change the pen size.
    const penOptions =
        annotationBar.shadowRoot!.querySelector('#pen viewer-pen-options');
    assert(penOptions);
    const sizeButton =
        penOptions.shadowRoot!.querySelector<HTMLElement>('#sizes [value="1"]');
    assert(sizeButton);
    sizeButton.click();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 0, b: 0},
      size: 1,
    });

    // Change the pen color.
    const colorButton = penOptions.shadowRoot!.querySelector<HTMLElement>(
        '#colors [value="#00b0ff"]');
    assert(colorButton);
    colorButton.click();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 176, b: 255},
      size: 1,
    });
    chrome.test.succeed();
  },
  // Test that the eraser can be selected.
  function testSelectEraser() {
    const annotationBar = getAnnotationsBar(viewerToolbar);

    // Switch to eraser. It shouldn't have any color.
    annotationBar.$.eraser.click();

    assertAnnotationBrush({type: AnnotationBrushType.ERASER, size: 1});
    chrome.test.succeed();
  },
  // Test that the pen can be selected again, and should have the same settings
  // as last set in `testSelectPen()`.
  function testGoBackToPenWithPreviousSettings() {
    const annotationBar = getAnnotationsBar(viewerToolbar);

    // Switch back to pen. It should have the previous settings.
    annotationBar.$.pen.click();

    assertAnnotationBrush({
      type: AnnotationBrushType.PEN,
      color: {r: 0, g: 176, b: 255},
      size: 1,
    });
    chrome.test.succeed();
  },
  // Test that the highlighter can be selected. Test that the dropdown menu can
  // be opened to select a color. Test that the size can be selected.
  function testSelectHighlighterWithDropdownColor() {
    const annotationBar = getAnnotationsBar(viewerToolbar);

    // Switch to highlighter.
    const highlighter = annotationBar.$.highlighter;
    highlighter.click();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {
        r: 255,
        g: 188,
        b: 0,
      },
      size: 0.7143,
    });

    // Fail to change the highlighter color to a hidden color. The
    // highlighter dropdown needs to be expanded to change to the color, so
    // without the dropdown, changing the color should fail.
    const highlighterOptions = highlighter.querySelector('viewer-pen-options');
    assert(highlighterOptions);
    const collapsedColorButton =
        highlighterOptions.shadowRoot!.querySelector<HTMLInputElement>(
            '#colors [value="#d1c4e9"]');
    assert(collapsedColorButton);
    chrome.test.assertTrue(collapsedColorButton.disabled);
    collapsedColorButton.click();

    // Open the dropdown menu and change the highlighter color.
    const dropdownMenu =
        highlighterOptions.shadowRoot!.querySelector<HTMLElement>(
            '#colors #expand');
    assert(dropdownMenu);
    dropdownMenu.click();

    chrome.test.assertFalse(collapsedColorButton.disabled);
    collapsedColorButton.click();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 209, g: 196, b: 233},
      size: 0.7143,
    });

    // Change the highlighter size.
    const sizeButton =
        highlighterOptions.shadowRoot!.querySelector<HTMLElement>(
            '#sizes [value="1"]');
    assert(sizeButton);
    sizeButton.click();

    assertAnnotationBrush({
      type: AnnotationBrushType.HIGHLIGHTER,
      color: {r: 209, g: 196, b: 233},
      size: 1,
    });
    chrome.test.succeed();
  },
  // Test the behavior of the undo and redo buttons.
  function testUndoRedo() {
    const annotationBar = getAnnotationsBar(viewerToolbar);
    const undoButton = annotationBar.$.undo;
    const redoButton = annotationBar.$.redo;

    // The buttons should be disabled when there aren't any strokes.
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Draw a stroke. The undo button should be enabled.
    finishInkStroke(controller);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Undo the stroke. The redo button should be enabled.
    undoButton.click();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    // Redo the stroke. The undo button should be enabled.
    mockPlugin.clearMessages();
    redoButton.click();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // After redo, draw a stroke and undo it after. The undo button and redo
    // button should both be enabled.
    mockPlugin.clearMessages();
    finishInkStroke(controller);
    undoButton.click();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    // Draw another stroke, overriding the stroke that could've been redone. The
    // undo button should be enabled.
    finishInkStroke(controller);

    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);
    chrome.test.succeed();
  },
]);
