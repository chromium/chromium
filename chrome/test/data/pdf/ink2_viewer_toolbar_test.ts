// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertCheckboxMenuButton, createMockPdfPluginForTest, enterFullscreenWithUserGesture, finishInkStroke, getRequiredElement, openToolbarMenu} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const controller = PluginController.getInstance();
const mockPlugin = createMockPdfPluginForTest();
controller.setPluginForTesting(mockPlugin);

function getUndoRedoModifier() {
  return isMac ? 'meta' : 'ctrl';
}

chrome.test.runTests([
  // Test that clicking the annotation button toggles annotation mode.
  async function testAnnotationButton() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const annotateButton = getRequiredElement(viewerToolbar, '#annotate');

    annotateButton.click();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    annotateButton.click();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode does not affect displaying annotations.
  async function testTogglingAnnotationModeDoesNotAffectDisplayAnnotations() {
    // The menu needs to be open to check for visible menu elements.
    await openToolbarMenu(viewerToolbar);

    // Start the test with annotation mode disabled and annotations displayed.
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    const showAnnotationsButton =
        getRequiredElement(viewerToolbar, '#show-annotations-button');
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);

    // Hide annotations.
    showAnnotationsButton.click();
    await microtasksFinished();

    // Clicking the button closes the menu, so re-open it.
    await openToolbarMenu(viewerToolbar);

    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode sends a message to the PDF content.
  async function testToggleAnnotationModeSendsMessage() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    const enableMessage = mockPlugin.findMessage('setAnnotationMode');
    chrome.test.assertTrue(enableMessage !== null);
    chrome.test.assertEq(enableMessage!.enable, true);

    mockPlugin.clearMessages();

    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const disableMessage = mockPlugin.findMessage('setAnnotationMode');
    chrome.test.assertTrue(disableMessage !== null);
    chrome.test.assertEq(disableMessage!.enable, false);
    chrome.test.succeed();
  },
  // Test that entering presentation mode exits annotation mode, and exiting
  // presentation mode re-enters annotation mode.
  async function testPresentationModeExitsAnnotationMode() {
    // First, check that there's no interaction with toggling presentation mode
    // when annotation mode is disabled.
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    await enterFullscreenWithUserGesture();
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    document.exitFullscreen();
    await eventToPromise('fullscreenchange', viewer.$.scroller);
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    // Now, check the interaction of toggling presentation mode when annotation
    // mode is enabled.
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    // Entering presentation mode should disable annotation mode.
    await enterFullscreenWithUserGesture();
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    // Exiting presentation mode should re-enable annotation mode.
    document.exitFullscreen();
    await eventToPromise('fullscreenchange', viewer.$.scroller);
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test the behavior of the undo and redo buttons.
  async function testUndoRedo() {
    mockPlugin.clearMessages();

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

    // The buttons should be disabled when there aren't any strokes.
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Draw a stroke. The undo button should be enabled.
    finishInkStroke(controller);
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Undo the stroke. The redo button should be enabled.
    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    // Redo the stroke. The undo button should be enabled.
    mockPlugin.clearMessages();
    redoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // After redo, draw a stroke and undo it after. The undo button and redo
    // button should both be enabled.
    mockPlugin.clearMessages();
    finishInkStroke(controller);
    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    // Draw another stroke, overriding the stroke that could've been redone. The
    // undo button should be enabled.
    finishInkStroke(controller);
    await microtasksFinished();

    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test that the undo and redo buttons are disabled when a text form field is
  // focused.
  async function testUndoRedoButtonsDisabledOnFormFieldFocus() {
    mockPlugin.clearMessages();

    chrome.test.assertTrue(viewerToolbar.annotationMode);

    // Exit annotation mode, since form fields can only be focused outside of
    // annotation mode.
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

    // Draw two strokes and undo, so that both undo and redo buttons are
    // enabled.
    finishInkStroke(controller);
    finishInkStroke(controller);
    await microtasksFinished();

    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    mockPlugin.clearMessages();

    // Simulate focusing on a text form field. Both buttons should be disabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'text'}}));
    await microtasksFinished();

    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Simulate focusing on a non-text form field. Both buttons should be
    // enabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'non-text'}}));
    await microtasksFinished();

    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    // Simulate removing focus from the form. Both buttons should be enabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'none'}}));
    await microtasksFinished();

    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test the behavior of the undo redo keyboard shortcuts.
  async function testUndoRedoKeyboardShortcuts() {
    mockPlugin.clearMessages();

    chrome.test.assertFalse(viewerToolbar.annotationMode);

    // Enable annotation mode.
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    finishInkStroke(controller);

    // Undo shortcut.
    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'z');

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);

    mockPlugin.clearMessages();

    // Redo shortcut.
    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'y');

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test that the undo and redo keyboard shortcuts are disabled when a text
  // form field is focused.
  async function testUndoRedoShortcutsDisabledOnFormFieldFocus() {
    mockPlugin.clearMessages();

    chrome.test.assertTrue(viewerToolbar.annotationMode);

    // Draw two strokes and undo, so that both undo and redo buttons are
    // enabled.
    finishInkStroke(controller);
    finishInkStroke(controller);
    await microtasksFinished();

    getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo').click();

    // Exit annotation mode, since form fields can only be focused outside of
    // annotation mode.
    viewerToolbar.toggleAnnotation();
    await microtasksFinished();
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    mockPlugin.clearMessages();

    // Simulate focusing on a text form field. Both shortcuts should be
    // disabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'text'}}));
    await microtasksFinished();

    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'z');
    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'y');

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);

    // Simulate focusing on a non-text form field. Both shortcuts should be
    // enabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'non-text'}}));
    await microtasksFinished();

    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'z');
    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'y');

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);

    mockPlugin.clearMessages();

    // Simulate removing focus from the form. Both shortcuts should be enabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'none'}}));
    await microtasksFinished();

    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'z');
    keyDownOn(viewerToolbar, 0, getUndoRedoModifier(), 'y');

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
]);
