// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrIconButtonElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

function createToolbar() {
  document.body.innerHTML = '';
  const toolbar = document.createElement('viewer-toolbar');
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
  function testEnteringAnnotationsModeDisablesPresentationMode() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    chrome.test.assertTrue(toolbar.$['present-button'].disabled);
    chrome.test.succeed();
  },
  function testEnteringAnnotationsModeDisablesTwoUp() {
    const toolbar = createToolbar();
    chrome.test.assertFalse(toolbar.annotationMode);

    toolbar.toggleAnnotation();
    // This is normally done by the parent in response to the event fired by
    // toggleAnnotation().
    toolbar.annotationMode = true;
    chrome.test.assertTrue(toolbar.$['two-page-view-button'].disabled);
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
    const annotateButton =
        toolbar.shadowRoot!.querySelector<CrIconButtonElement>('#annotate');
    chrome.test.assertTrue(!!annotateButton);
    chrome.test.assertFalse(annotateButton.disabled);
    annotateButton.click();
    await waitBeforeNextRender(toolbar);
    let dialog =
        toolbar.shadowRoot!.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(!!dialog);
    chrome.test.assertTrue(dialog.isOpen());

    // Cancel the dialog.
    const whenClosed = eventToPromise('close', dialog);
    dialog.shadowRoot!.querySelector<HTMLElement>('.cancel-button')!.click();
    chrome.test.assertFalse(dialog.isOpen());
    await whenClosed;

    // If both two up and rotate are enabled, the dialog opens.
    toolbar.twoUpViewEnabled = true;
    chrome.test.assertFalse(annotateButton.disabled);
    annotateButton.click();
    await waitBeforeNextRender(toolbar);
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
    await waitBeforeNextRender(toolbar);

    // Dialog shows in two up view (un-rotated).
    toolbar.rotated = false;
    chrome.test.assertFalse(annotateButton.disabled);
    annotateButton.click();
    await waitBeforeNextRender(toolbar);
    dialog =
        toolbar.shadowRoot!.querySelector('viewer-annotations-mode-dialog');
    chrome.test.assertTrue(!!dialog);
    chrome.test.assertTrue(dialog.isOpen());
    chrome.test.succeed();
  },
];

chrome.test.runTests(tests);
