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

suite('Ink2SignatureDialogTest', () => {
  test('OpenAndCloseSignatureDialog', async () => {
    assertEquals(AnnotationMode.OFF, viewer.$.toolbar.annotationMode);
    assertFalse(!!getSignatureDialog());

    // Entering signature mode opens the signature dialog.
    const whenOpen = eventToPromise('cr-dialog-open', viewer);
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.SIGNATURE);
    await whenOpen;
    await microtasksFinished();

    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
    const dialog = getSignatureDialog();
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
    assertTrue(isVisible(dialog.$.close));
    assertTrue(isVisible(dialog.$.clear));
    assertTrue(isVisible(dialog.$.create));

    // Clicking the close button closes the dialog while staying in signature
    // mode.
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.close.click();
    await whenClosed;
    await microtasksFinished();

    assertFalse(!!getSignatureDialog());
    assertEquals(AnnotationMode.SIGNATURE, viewer.$.toolbar.annotationMode);
  });

  test('ReopenSignatureDialog', async () => {
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.OFF);
    await microtasksFinished();
    assertFalse(!!getSignatureDialog());

    // Re-entering signature mode opens the dialog again.
    const whenOpen = eventToPromise('cr-dialog-open', viewer);
    viewer.$.toolbar.setAnnotationMode(AnnotationMode.SIGNATURE);
    await whenOpen;
    await microtasksFinished();

    const dialog = getSignatureDialog();
    assertTrue(!!dialog);
    assertTrue(dialog.$.dialog.open);
  });
});
