// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {eventToPromise, waitBeforeNextRender} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/_test_resources/webui/test_util.m.js';
import {ViewerPdfToolbarNewElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';

/** @return {!ViewerPdfToolbarNewElement} */
function createToolbar() {
  document.body.innerHTML = '';
  const toolbar = /** @type {!ViewerPdfToolbarNewElement} */ (
      document.createElement('viewer-pdf-toolbar-new'));
  document.body.appendChild(toolbar);
  return toolbar;
}

const tests = [
  function testHidingAnnotationsExitsAnnotationsMode() {
    const toolbar = createToolbar();
    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;

    toolbar.addEventListener('display-annotations-changed', async e => {
      chrome.test.assertFalse(e.detail);
      chrome.test.succeed();
    });
    toolbar.shadowRoot.querySelector('#show-annotations-button').click();
  },
  function testEnteringAnnotationsModeShowsAnnotations() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    // Hide annotations.
    toolbar.shadowRoot.querySelector('#show-annotations-button').click();

    toolbar.addEventListener('annotation-mode-toggled', e => {
      chrome.test.assertTrue(e.detail);
      chrome.test.succeed();
    });
    toolbar.toggleAnnotation();
  },
  function testEnteringAnnotationsModeDisablesTwoUp() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    chrome.test.assertTrue(
        toolbar.shadowRoot.querySelector('#two-page-view-button').disabled);
    chrome.test.succeed();
  },
  async function testRotateOrTwoUpViewTriggersDialog() {
    const toolbar = createToolbar();
    toolbar.annotationAvailable = true;
    toolbar.pdfAnnotationsEnabled = true;
    toolbar.rotated = false;
    toolbar.twoUpViewEnabled = false;

    await waitBeforeNextRender(toolbar);
    chrome.test.assertFalse(toolbar.annotationMode);

    // If rotation is enabled clicking the button shows the dialog.
    toolbar.rotated = true;
    const annotateButton = toolbar.shadowRoot.querySelector('#annotate');
    chrome.test.assertFalse(annotateButton.disabled);
    annotateButton.click();
    await waitBeforeNextRender(toolbar);
    let dialog =
        toolbar.shadowRoot.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(dialog.isOpen());

    // Cancel the dialog.
    const whenClosed = eventToPromise('close', dialog);
    dialog.shadowRoot.querySelector('.cancel-button').click();
    chrome.test.assertFalse(dialog.isOpen());
    await whenClosed;

    // If both two up and rotate are enabled, the dialog opens.
    toolbar.twoUpViewEnabled = true;
    chrome.test.assertFalse(annotateButton.disabled);
    annotateButton.click();
    await waitBeforeNextRender(toolbar);
    dialog = toolbar.shadowRoot.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(dialog.isOpen());

    // When "Edit" is clicked, the toolbar should fire
    // annotation-mode-dialog-confirmed.
    const whenConfirmed =
        eventToPromise('annotation-mode-dialog-confirmed', toolbar);
    dialog.shadowRoot.querySelector('.action-button').click();
    await whenConfirmed;
    chrome.test.assertFalse(dialog.isOpen());
    await waitBeforeNextRender(toolbar);

    // Dialog shows in two up view (un-rotated).
    toolbar.rotated = false;
    chrome.test.assertFalse(annotateButton.disabled);
    annotateButton.click();
    await waitBeforeNextRender(toolbar);
    dialog = toolbar.shadowRoot.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(dialog.isOpen());
    chrome.test.succeed();
  }
];

chrome.test.runTests(tests);
