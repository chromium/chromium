// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode, PluginController, UserAction} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {InkTextBoxElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {isMac} from 'chrome://resources/js/platform.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {assertCheckboxMenuButton, createTextBox, enterFullscreenWithUserGesture, finishInkStroke, getRequiredElement, openToolbarMenu, setupMockMetricsPrivate, setupTestMockPluginForInk, startFinishModifiedInkStroke, startInkStroke} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;
const controller = PluginController.getInstance();
const mockPlugin = setupTestMockPluginForInk();
const mockMetricsPrivate = setupMockMetricsPrivate();

function sendUndoShortcutKey(target: Element) {
  keyDownOn(target, 0, isMac ? 'meta' : 'ctrl', 'z');
}

function sendRedoShortcutKey(target: Element) {
  if (isMac) {
    keyDownOn(target, 0, ['meta', 'shift'], 'z');
  } else {
    keyDownOn(target, 0, 'ctrl', 'y');
  }
}

// Utils to add extra wait for Mac13 tests.
async function createTextBoxAndWaitForStateChange(textBox: HTMLElement) {
  const whenStateChanged = eventToPromise('state-changed', textBox);
  createTextBox();
  await whenStateChanged;
  await microtasksFinished();
}

async function commitAnnotationAndWaitForStateChange(
    textBox: InkTextBoxElement) {
  const whenStateChanged = eventToPromise('state-changed', textBox);
  textBox.commitTextAnnotation();
  await whenStateChanged;
  await microtasksFinished();
}

chrome.test.runTests([
  // Test that clicking the annotation button toggles annotation mode.
  async function testAnnotationButton() {
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    const annotateButton = getRequiredElement(viewerToolbar, '#annotate');

    annotateButton.click();
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    annotateButton.click();
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test that clicking the text annotation button toggles text annotation mode.
  async function testTextAnnotationButton() {
    // No button if feature param is not enabled.
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': false});
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();
    chrome.test.assertFalse(
        !!viewerToolbar.shadowRoot.querySelector('#text-annotate'));

    // Set the feature param in loadTimeData and trigger Lit binding.
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': true});
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    // Button should now exist. Clicking the text annotation button enables
    // text annotation mode.
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    const textButton = getRequiredElement(viewerToolbar, '#text-annotate');
    textButton.click();
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);

    // Clicking the draw annotation button while text is enabled switches to
    // draw mode.
    const annotateButton = getRequiredElement(viewerToolbar, '#annotate');
    annotateButton.click();
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    // Clicking text annotation button while drawing is enabled switches to
    // text mode.
    textButton.click();
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);

    // Clicking the text button while in text mode exits annotation mode.
    textButton.click();
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode does not affect displaying annotations.
  async function testTogglingAnnotationModeDoesNotAffectDisplayAnnotations() {
    // The menu needs to be open to check for visible menu elements.
    await openToolbarMenu(viewerToolbar);

    // Start the test with annotation mode disabled and annotations displayed.
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    const showAnnotationsButton =
        getRequiredElement(viewerToolbar, '#show-annotations-button');
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);
    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);

    // Hide annotations.
    showAnnotationsButton.click();
    await microtasksFinished();

    // Clicking the button closes the menu, so re-open it.
    await openToolbarMenu(viewerToolbar);

    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);
    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode sends a message to the PDF content.
  async function testToggleAnnotationModeSendsMessage() {
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    let enableMessage = mockPlugin.findMessage('setAnnotationMode');
    chrome.test.assertTrue(enableMessage !== null);
    chrome.test.assertEq(enableMessage!.mode, AnnotationMode.DRAW);

    mockPlugin.clearMessages();

    viewerToolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);

    enableMessage = mockPlugin.findMessage('setAnnotationMode');
    chrome.test.assertTrue(enableMessage !== null);
    chrome.test.assertEq(enableMessage!.mode, AnnotationMode.TEXT);

    mockPlugin.clearMessages();

    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    const disableMessage = mockPlugin.findMessage('setAnnotationMode');
    chrome.test.assertTrue(disableMessage !== null);
    chrome.test.assertEq(disableMessage!.mode, AnnotationMode.OFF);
    chrome.test.succeed();
  },
  // Test that entering presentation mode exits annotation mode, and exiting
  // presentation mode re-enters annotation mode.
  async function testPresentationModeExitsAnnotationMode() {
    // First, check that there's no interaction with toggling presentation mode
    // when annotation mode is disabled.
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    await enterFullscreenWithUserGesture();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    document.exitFullscreen();
    await eventToPromise('fullscreenchange', viewer.$.scroller);
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    // Now, check the interaction of toggling presentation mode when annotation
    // mode is enabled.
    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    // Entering presentation mode should disable annotation mode.
    await enterFullscreenWithUserGesture();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    // Exiting presentation mode should re-enable annotation mode.
    document.exitFullscreen();
    await eventToPromise('fullscreenchange', viewer.$.scroller);
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test the behavior of the undo and redo buttons.
  async function testUndoRedo() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

    // The buttons should be disabled when there aren't any strokes.
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Perform a stroke that did not modify anything. The undo/redo state should
    // not change.
    startInkStroke(controller);
    finishInkStroke(controller, false);
    await microtasksFinished();

    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Draw a stroke. The undo button should be enabled.
    startFinishModifiedInkStroke(controller);
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
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Perform a stroke that did not modify anything. The undo/redo state should
    // not change.
    startInkStroke(controller);
    finishInkStroke(controller, false);
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Redo the stroke. The undo button should be enabled.
    mockPlugin.clearMessages();
    redoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);

    // After redo, draw a stroke and undo it after. The undo button and redo
    // button should both be enabled.
    mockPlugin.clearMessages();
    startFinishModifiedInkStroke(controller);
    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);

    // Draw another stroke, overriding the stroke that could've been redone. The
    // undo button should be enabled.
    startFinishModifiedInkStroke(controller);
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

    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    // Exit annotation mode, since form fields can only be focused outside of
    // annotation mode.
    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

    // Draw two strokes and undo, so that both undo and redo buttons are
    // enabled.
    startFinishModifiedInkStroke(controller);
    startFinishModifiedInkStroke(controller);
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
  async function testUndoRedoTextAnnotation() {
    // Set the feature param in loadTimeData and trigger Lit binding.
    loadTimeData.overrideValues({'pdfTextAnnotationsEnabled': true});
    viewerToolbar.strings = Object.assign({}, viewerToolbar.strings);
    await microtasksFinished();

    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Enter draw mode to draw a stroke.
    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

    // The buttons should be disabled when there aren't any changes.
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Draw a stroke. The undo button should be enabled.
    startFinishModifiedInkStroke(controller);
    await microtasksFinished();
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Button still enabled after changing to text annotation mode.
    viewerToolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Create a textbox. The undo button should now be disabled.
    const textBox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertTrue(!!textBox);
    chrome.test.assertFalse(isVisible(textBox));
    await createTextBoxAndWaitForStateChange(textBox);
    chrome.test.assertTrue(isVisible(textBox));
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Simulate closing the textbox with no changes. Now the undo button is
    // enabled again.
    await commitAnnotationAndWaitForStateChange(textBox);
    chrome.test.assertFalse(isVisible(textBox));
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Undo the stroke. The redo button should be enabled.
    undoButton.click();
    await microtasksFinished();
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Add a textbox. The redo button is disabled.
    mockPlugin.clearMessages();
    await createTextBoxAndWaitForStateChange(textBox);
    chrome.test.assertTrue(isVisible(textBox));
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Make a change to the textbox before closing. Now the undo button
    // should be enabled, since there is a new text annotation change. Redo
    // is disabled since the new text annotation overrides the stroke that
    // could have been redone.
    const whenStateChanged = eventToPromise('state-changed', textBox);
    textBox.$.textbox.value = 'Hello';
    textBox.$.textbox.dispatchEvent(new CustomEvent('input'));
    // Wait for textbox state edited.
    await whenStateChanged;
    await microtasksFinished();
    await commitAnnotationAndWaitForStateChange(textBox);
    chrome.test.assertFalse(isVisible(textBox));
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Reset state for later tests.
    viewerToolbar.resetStrokesForTesting();
    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.succeed();
  },
  // Test that the undo and redo buttons are active but do nothing when a stroke
  // is in progress.
  async function testUndoRedoButtonsAreNoopsWhenStrokeInProgress() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

    // The buttons should be disabled when there aren't any strokes.
    chrome.test.assertTrue(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);

    // Draw 2 strokes and undo one. The undo/redo buttons should be enabled.
    startFinishModifiedInkStroke(controller);
    startFinishModifiedInkStroke(controller);
    await microtasksFinished();
    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Start drawing another stroke, but do not finish it yet.
    mockPlugin.clearMessages();
    startInkStroke(controller);
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Try to undo, which should do nothing even though the button is enabled.
    mockPlugin.clearMessages();
    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Try to redo, which should do nothing even though the button is enabled.
    mockPlugin.clearMessages();
    redoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Finish the stroke. Redo button is no longer enabled.
    mockPlugin.clearMessages();
    finishInkStroke(controller, true);
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Make sure undo works. Then both buttons are enabled.
    mockPlugin.clearMessages();
    undoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertFalse(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Make sure redo works. Then redo button is disabled again.
    mockPlugin.clearMessages();
    redoButton.click();
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    chrome.test.assertFalse(undoButton.disabled);
    chrome.test.assertTrue(redoButton.disabled);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test the behavior of the undo redo keyboard shortcuts.
  async function testUndoRedoKeyboardShortcuts() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    // Enable annotation mode.
    viewerToolbar.setAnnotationMode(AnnotationMode.DRAW);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    startFinishModifiedInkStroke(controller);

    sendUndoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    mockPlugin.clearMessages();

    sendRedoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test that the undo and redo keyboard shortcuts are disabled when a text
  // form field is focused.
  async function testUndoRedoShortcutsDisabledOnFormFieldFocus() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    chrome.test.assertEq(AnnotationMode.DRAW, viewerToolbar.annotationMode);

    // Draw two strokes and undo, so that both undo and redo buttons are
    // enabled.
    startFinishModifiedInkStroke(controller);
    startFinishModifiedInkStroke(controller);
    await microtasksFinished();

    getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo').click();

    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);

    // Exit annotation mode, since form fields can only be focused outside of
    // annotation mode.
    viewerToolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    mockPlugin.clearMessages();

    // Simulate focusing on a text form field. Both shortcuts should be
    // disabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'text'}}));
    await microtasksFinished();

    sendUndoShortcutKey(viewerToolbar);
    sendRedoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Simulate focusing on a non-text form field. Both shortcuts should be
    // enabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'non-text'}}));
    await microtasksFinished();

    sendUndoShortcutKey(viewerToolbar);
    sendRedoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);

    mockPlugin.clearMessages();

    // Simulate removing focus from the form. Both shortcuts should be enabled.
    mockPlugin.dispatchEvent(new MessageEvent(
        'message', {data: {type: 'formFocusChange', focused: 'none'}}));
    await microtasksFinished();

    sendUndoShortcutKey(viewerToolbar);
    sendRedoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 3);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 2);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test the behavior of the undo redo keyboard shortcuts in text annotation
  // mode.
  async function testUndoRedoKeyboardShortcutsTextAnnotation() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    chrome.test.assertEq(AnnotationMode.OFF, viewerToolbar.annotationMode);

    // Enable text annotation mode.
    viewerToolbar.setAnnotationMode(AnnotationMode.TEXT);
    await microtasksFinished();
    chrome.test.assertEq(AnnotationMode.TEXT, viewerToolbar.annotationMode);

    // Simulate committing an edited text annotation.
    startFinishModifiedInkStroke(controller);

    sendUndoShortcutKey(viewerToolbar);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);
    mockPlugin.clearMessages();

    sendRedoShortcutKey(viewerToolbar);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);
    mockPlugin.clearMessages();

    // Shortcuts don't work when there is an active text box (instead, they
    // are handled by the native <textarea> element).
    const textBox = viewer.shadowRoot.querySelector('ink-text-box');
    chrome.test.assertTrue(!!textBox);
    await createTextBoxAndWaitForStateChange(textBox);
    sendUndoShortcutKey(viewerToolbar);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);
    mockPlugin.clearMessages();

    // Close textbox. Undo works again.
    await commitAnnotationAndWaitForStateChange(textBox);
    sendUndoShortcutKey(viewerToolbar);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);
    mockPlugin.clearMessages();

    // Redo also doesn't work with a textbox open.
    await createTextBoxAndWaitForStateChange(textBox);
    sendRedoShortcutKey(viewerToolbar);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);
    mockPlugin.clearMessages();

    // Close textbox. Redo works again.
    await commitAnnotationAndWaitForStateChange(textBox);
    sendRedoShortcutKey(viewerToolbar);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 2);
    mockPlugin.clearMessages();

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
  // Test that the undo and redo keyboard shortcuts do nothing when a stroke is
  // in progress.
  async function testUndoRedoKeyboardShortcutsAreNoopsWhenStrokeInProgress() {
    mockPlugin.clearMessages();
    mockMetricsPrivate.reset();

    // Draw 2 strokes and undo one. The undo/redo buttons should be enabled.
    startFinishModifiedInkStroke(controller);
    startFinishModifiedInkStroke(controller);
    await microtasksFinished();
    sendUndoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Start drawing another stroke, but do not finish it yet.
    mockPlugin.clearMessages();
    startInkStroke(controller);
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Try to undo, which should do nothing.
    mockPlugin.clearMessages();
    sendUndoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Try to redo, which should do nothing.
    mockPlugin.clearMessages();
    sendRedoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Finish the stroke. Redo is no longer possible.
    mockPlugin.clearMessages();
    finishInkStroke(controller, true);
    await microtasksFinished();

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 1);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Make sure undo works.
    mockPlugin.clearMessages();
    sendUndoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') !== undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') === undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 0);

    // Make sure redo works.
    mockPlugin.clearMessages();
    sendRedoShortcutKey(viewerToolbar);

    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationUndo') === undefined);
    chrome.test.assertTrue(
        mockPlugin.findMessage('annotationRedo') !== undefined);
    mockMetricsPrivate.assertCount(UserAction.UNDO_INK2, 2);
    mockMetricsPrivate.assertCount(UserAction.REDO_INK2, 1);

    viewerToolbar.resetStrokesForTesting();
    chrome.test.succeed();
  },
]);
