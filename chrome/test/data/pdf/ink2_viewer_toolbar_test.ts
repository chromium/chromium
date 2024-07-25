// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertCheckboxMenuButton, createMockPdfPluginForTest, enterFullscreenWithUserGesture, finishInkStroke, getRequiredElement, openToolbarMenu} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;

chrome.test.runTests([
  // Test that clicking the annotation button toggles annotation mode.
  function testAnnotationButton() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const annotateButton = getRequiredElement(viewerToolbar, '#annotate');

    annotateButton.click();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    annotateButton.click();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode does not affect displaying annotations.
  function testTogglingAnnotationModeDoesNotAffectDisplayAnnotations() {
    // The menu needs to be open to check for visible menu elements.
    openToolbarMenu(viewerToolbar);

    // Start the test with annotation mode disabled and annotations displayed.
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    const showAnnotationsButton =
        getRequiredElement(viewerToolbar, '#show-annotations-button');
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);
    viewerToolbar.toggleAnnotation();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, true);

    // Hide annotations.
    showAnnotationsButton.click();

    // Clicking the button closes the menu, so re-open it.
    openToolbarMenu(viewerToolbar);

    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);
    viewerToolbar.toggleAnnotation();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    assertCheckboxMenuButton(viewerToolbar, showAnnotationsButton, false);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode sends a message to the PDF content.
  async function testToggleAnnotationModeSendsMessage() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const controller = PluginController.getInstance();
    const mockPlugin = createMockPdfPluginForTest();
    controller.setPluginForTesting(mockPlugin);

    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    const enableMessage = mockPlugin.findMessage('setAnnotationMode');
    chrome.test.assertTrue(enableMessage !== null);
    chrome.test.assertEq(enableMessage!.enable, true);

    mockPlugin.clearMessages();

    viewerToolbar.toggleAnnotation();
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
  function testUndoRedo() {
    const controller = PluginController.getInstance();
    const mockPlugin = createMockPdfPluginForTest();
    controller.setPluginForTesting(mockPlugin);

    const undoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#undo');
    const redoButton =
        getRequiredElement<HTMLButtonElement>(viewerToolbar, '#redo');

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
