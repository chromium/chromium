// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationBrushParams} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {AnnotationBrushType, PluginController, PluginControllerEventType} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assert} from 'chrome://resources/js/assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {createMockPdfPluginForTest} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const controller = PluginController.getInstance();
const mockPlugin = createMockPdfPluginForTest();
controller.setPluginForTesting(mockPlugin);

/**
 * Test that the annotation brush is of type `type`, and has the `params`, if
 * necessary. Clears all messages from `mockPlugin` after, otherwise subsequent
 * calls would continue to find and use the same message.
 * @param type The type of the annotation brush.
 * @param params The params for the annotation brush, if necessary for the type
 *     of brush.
 */
function assertAnnotationBrush(
    type: AnnotationBrushType, params?: AnnotationBrushParams) {
  const setAnnotationBrushMessage =
      mockPlugin.findMessage('setAnnotationBrush');
  chrome.test.assertTrue(setAnnotationBrushMessage !== undefined);
  chrome.test.assertEq('setAnnotationBrush', setAnnotationBrushMessage.type);
  chrome.test.assertEq(type, setAnnotationBrushMessage.brushType);
  const hasParams = params !== undefined;
  chrome.test.assertEq(
      hasParams ? params.colorR : undefined, setAnnotationBrushMessage.colorR);
  chrome.test.assertEq(
      hasParams ? params.colorG : undefined, setAnnotationBrushMessage.colorG);
  chrome.test.assertEq(
      hasParams ? params.colorB : undefined, setAnnotationBrushMessage.colorB);
  chrome.test.assertEq(
      hasParams ? params.size : undefined, setAnnotationBrushMessage.size);

  mockPlugin.clearMessages();
}

/**
 * Helper to always got a non-null annotation bar. The annotation bar must
 * exist.
 * @returns The annotation bar.
 */
function getAnnotationsBar() {
  const annotationsBar =
      viewerToolbar.shadowRoot!.querySelector('viewer-annotations-bar');
  assert(annotationsBar);
  return annotationsBar;
}

/**
 * Helper to simulate the PDF content sending a message to the PDF extension
 * to indicate that a new ink stroke has been drawn.
 */
function finishInkStroke() {
  const eventTarget = controller.getEventTarget();
  const message = {type: 'finishInkStroke'};

  eventTarget.dispatchEvent(new CustomEvent(
      PluginControllerEventType.PLUGIN_MESSAGE, {detail: message}));
}

chrome.test.runTests([
  // Test that the annotations bar is shown when annotation mode is enabled and
  // hidden when annotation mode is disabled.
  async function testAnnotationBar() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    await waitAfterNextRender(viewerToolbar);
    const annotationsBar = getAnnotationsBar();

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

    const annotationBar = getAnnotationsBar();

    // Default to pen.
    assertAnnotationBrush(
        AnnotationBrushType.PEN,
        {colorR: 0, colorG: 0, colorB: 0, size: 0.1429});

    // Change the pen size.
    const penOptions =
        annotationBar.shadowRoot!.querySelector('#pen viewer-pen-options');
    assert(penOptions);
    const sizeButton =
        penOptions.shadowRoot!.querySelector<HTMLElement>('#sizes [value="1"]');
    assert(sizeButton);
    sizeButton.click();

    assertAnnotationBrush(
        AnnotationBrushType.PEN, {colorR: 0, colorG: 0, colorB: 0, size: 1});

    // Change the pen color.
    const colorButton = penOptions.shadowRoot!.querySelector<HTMLElement>(
        '#colors [value="#00b0ff"]');
    assert(colorButton);
    colorButton.click();

    assertAnnotationBrush(
        AnnotationBrushType.PEN,
        {colorR: 0, colorG: 176, colorB: 255, size: 1});
    chrome.test.succeed();
  },
  // Test that the eraser can be selected.
  function testSelectEraser() {
    const annotationBar = getAnnotationsBar();

    // Switch to eraser. It shouldn't have any params.
    annotationBar.$.eraser.click();

    assertAnnotationBrush(AnnotationBrushType.ERASER);
    chrome.test.succeed();
  },
  // Test that the pen can be selected again, and should have the same settings
  // as last set in `testSelectPen()`.
  function testGoBackToPenWithPreviousSettings() {
    const annotationBar = getAnnotationsBar();

    // Switch back to pen. It should have the previous settings.
    annotationBar.$.pen.click();

    assertAnnotationBrush(
        AnnotationBrushType.PEN,
        {colorR: 0, colorG: 176, colorB: 255, size: 1});
    chrome.test.succeed();
  },
  // Test that the highlighter can be selected. Test that the dropdown menu can
  // be opened to select a color. Test that the size can be selected.
  function testSelectHighlighterWithDropdownColor() {
    const annotationBar = getAnnotationsBar();

    // Switch to highlighter.
    const highlighter = annotationBar.$.highlighter;
    highlighter.click();

    assertAnnotationBrush(
        AnnotationBrushType.HIGHLIGHTER,
        {colorR: 255, colorG: 188, colorB: 0, size: 0.7143});

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

    assertAnnotationBrush(
        AnnotationBrushType.HIGHLIGHTER,
        {colorR: 209, colorG: 196, colorB: 233, size: 0.7143});

    // Change the highlighter size.
    const sizeButton =
        highlighterOptions.shadowRoot!.querySelector<HTMLElement>(
            '#sizes [value="1"]');
    assert(sizeButton);
    sizeButton.click();

    assertAnnotationBrush(
        AnnotationBrushType.HIGHLIGHTER,
        {colorR: 209, colorG: 196, colorB: 233, size: 1});
    chrome.test.succeed();
  },
  // Test the behavior of the undo and redo buttons.
  function testUndoRedo() {
    const annotationBar = getAnnotationsBar();
    const undoButton = annotationBar.$.undo;
    const redoButton = annotationBar.$.redo;

    // The buttons should be disabled when there aren't any strokes.
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Draw a stroke. The undo button should be enabled.
    finishInkStroke();

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
    finishInkStroke();
    undoButton.click();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    // Draw another stroke, overriding the stroke that could've been redone. The
    // undo button should be enabled.
    finishInkStroke();

    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);
    chrome.test.succeed();
  },
]);
