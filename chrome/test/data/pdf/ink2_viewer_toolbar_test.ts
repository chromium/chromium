// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertShowAnnotationsButton} from './test_util.js';

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
]);
