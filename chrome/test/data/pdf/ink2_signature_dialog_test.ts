// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AnnotationMode} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import type {ViewerSignatureDialogElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {setupTestMockPluginForInk} from './test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;
setupTestMockPluginForInk();

function getSignatureDialog(): ViewerSignatureDialogElement|null {
  return viewer.shadowRoot.querySelector<ViewerSignatureDialogElement>(
      'viewer-signature-dialog');
}

async function openSignatureDialog(): Promise<ViewerSignatureDialogElement> {
  const whenOpen = eventToPromise('cr-dialog-open', viewer);
  viewer.$.toolbar.setAnnotationMode(AnnotationMode.SIGNATURE);
  await whenOpen;
  await microtasksFinished();
  const dialog = getSignatureDialog();
  assertTrue(!!dialog);
  return dialog;
}

suite('Ink2SignatureDialogTest', function() {
  setup(async function() {
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
  });

  test('OpenAndCloseSignatureDialog', async function() {
    assertEquals(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    assertFalse(!!getSignatureDialog());

    // Entering signature mode opens the signature dialog.
    let dialog = await openSignatureDialog();
    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
    assertTrue(dialog.$.dialog.open);
    assertTrue(isVisible(dialog.$.canvas));
    assertTrue(isVisible(dialog.$.clear));
    assertTrue(isVisible(dialog.$.create));

    // Clicking the close button closes the dialog while staying in signature
    // mode.
    const closeButton =
        dialog.$.dialog.shadowRoot.querySelector<HTMLElement>('#close');
    assertTrue(!!closeButton);
    const whenClosed = eventToPromise('close', dialog);
    closeButton.click();
    await whenClosed;
    await microtasksFinished();

    assertFalse(!!getSignatureDialog());
    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);

    // Exiting and re-entering signature mode opens the dialog again.
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    assertFalse(!!getSignatureDialog());

    dialog = await openSignatureDialog();
    assertTrue(dialog.$.dialog.open);
  });
});
