// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PluginController} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {assertShowAnnotationsButton, createMockPdfPluginForTest, enterFullscreenWithUserGesture} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
const viewerToolbar = viewer.$.toolbar;

function toolbarQuerySelector(query: string): HTMLElement {
  return viewerToolbar.shadowRoot!.querySelector<HTMLElement>(query)!;
}

chrome.test.runTests([
  // Test that clicking the annotation button toggles annotation mode.
  function testAnnotationButton() {
    chrome.test.assertFalse(viewerToolbar.annotationMode);

    const annotateButton = toolbarQuerySelector('#annotate');

    annotateButton.click();
    chrome.test.assertTrue(viewerToolbar.annotationMode);

    annotateButton.click();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    chrome.test.succeed();
  },
  // Test that toggling annotation mode does not affect displaying annotations.
  function testTogglingAnnotationModeDoesNotAffectDisplayAnnotations() {
    // Start the test with annotation mode disabled and annotations displayed.
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    const showAnnotationsButton =
        toolbarQuerySelector('#show-annotations-button');
    assertShowAnnotationsButton(showAnnotationsButton, true);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    assertShowAnnotationsButton(showAnnotationsButton, true);
    viewerToolbar.toggleAnnotation();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    assertShowAnnotationsButton(showAnnotationsButton, true);

    // Hide annotations.
    showAnnotationsButton.click();
    assertShowAnnotationsButton(showAnnotationsButton, false);

    // Enabling and disabling annotation mode shouldn't affect displaying
    // annotations.
    viewerToolbar.toggleAnnotation();
    chrome.test.assertTrue(viewerToolbar.annotationMode);
    assertShowAnnotationsButton(showAnnotationsButton, false);
    viewerToolbar.toggleAnnotation();
    chrome.test.assertFalse(viewerToolbar.annotationMode);
    assertShowAnnotationsButton(showAnnotationsButton, false);
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
]);
