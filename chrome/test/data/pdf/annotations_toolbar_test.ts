// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

function createToolbar() {
  document.body.innerHTML = '';
  const toolbar = document.createElement('viewer-toolbar');
  document.body.appendChild(toolbar);
  return toolbar;
}

const tests = [
  async function testHidingAnnotationsExitsAnnotationsMode() {
    const toolbar = createToolbar();
    toolbar.toggleAnnotation();

    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    await microtasksFinished();

    toolbar.addEventListener('display-annotations-changed', e => {
      chrome.test.assertFalse(e.detail);
      chrome.test.succeed();
    });
    toolbar.shadowRoot!.querySelector<HTMLElement>(
                           '#show-annotations-button')!.click();
  },
  function testEnteringAnnotationsModeShowsAnnotations() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    // Hide annotations.
    toolbar.shadowRoot!.querySelector<HTMLElement>(
                           '#show-annotations-button')!.click();

    toolbar.addEventListener('annotation-mode-toggled', e => {
      chrome.test.assertTrue(e.detail);
      chrome.test.succeed();
    });
    toolbar.toggleAnnotation();
  },
  async function testEnteringAnnotationsModeDisablesPresentationMode() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    await microtasksFinished();
    chrome.test.assertTrue(toolbar.$['present-button'].disabled);
    chrome.test.succeed();
  },
  async function testEnteringAnnotationsModeDisablesTwoUp() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    await microtasksFinished();
    chrome.test.assertTrue(toolbar.$['two-page-view-button'].disabled);
    chrome.test.succeed();
  },
  async function testRotateOrTwoUpViewTriggersDialog() {
    const toolbar = createToolbar();
    toolbar.annotationAvailable = true;
    toolbar.pdfAnnotationsEnabled = true;
    toolbar.rotated = false;
    toolbar.twoUpViewEnabled = false;

    await microtasksFinished();
    chrome.test.assertFalse(toolbar.annotationMode);

    // If rotation is enabled clicking the button shows the dialog.
    toolbar.rotated = true;
    const annotateButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#annotate');
    chrome.test.assertTrue(!!annotateButton);
    chrome.test.assertFalse(annotateButton.disabled);
    // Listen for a 'cr-dialog-open' event on the toolbar itself, since the
    // dialog does not exist yet.
    let whenOpen = eventToPromise('cr-dialog-open', toolbar);
    annotateButton.click();
    await whenOpen;
    let dialog =
        toolbar.shadowRoot!.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(!!dialog);
    chrome.test.assertTrue(dialog.isOpen());

    // Cancel the dialog.
    const whenClosed = eventToPromise('close', dialog);
    dialog.shadowRoot!.querySelector<HTMLElement>('.cancel-button')!.click();
    await whenClosed;
    chrome.test.assertFalse(dialog.isOpen());

    // If both two up and rotate are enabled, the dialog opens.
    toolbar.twoUpViewEnabled = true;
    await microtasksFinished();
    chrome.test.assertFalse(annotateButton.disabled);
    whenOpen = eventToPromise('cr-dialog-open', toolbar);
    annotateButton.click();
    await whenOpen;
    dialog =
        toolbar.shadowRoot!.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(!!dialog);
    chrome.test.assertTrue(dialog.isOpen());

    // When "Edit" is clicked, the toolbar should fire
    // annotation-mode-dialog-confirmed.
    const whenConfirmed =
        eventToPromise('annotation-mode-dialog-confirmed', toolbar);
    dialog.shadowRoot!.querySelector<HTMLElement>('.action-button')!.click();
    await whenConfirmed;
    chrome.test.assertFalse(dialog.isOpen());

    // Dialog shows in two up view (un-rotated).
    toolbar.rotated = false;
    await microtasksFinished();
    chrome.test.assertFalse(annotateButton.disabled);
    whenOpen = eventToPromise('cr-dialog-open', toolbar);
    annotateButton.click();
    await whenOpen;
    dialog =
        toolbar.shadowRoot!.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(!!dialog);
    chrome.test.assertTrue(dialog.isOpen());
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
