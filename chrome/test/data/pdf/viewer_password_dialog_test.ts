// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ViewerPasswordDialogElement} from 'chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/pdf_viewer_wrapper.js';
import {assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

const viewer = document.body.querySelector('pdf-viewer')!;

function getPasswordDialog() {
  return viewer.shadowRoot.querySelector('viewer-password-dialog');
}

function submitPassword(dialog: ViewerPasswordDialogElement) {
  dialog.$.submit.click();

  // The submit button and input field should both be disabled.
  assertTrue(dialog.$.submit.disabled);
  assertTrue(dialog.$.password.disabled);
}

async function tryIncorrectPassword(
    dialog: ViewerPasswordDialogElement, password: string) {
  dialog.$.password.value = password;

  const whenPasswordDenied =
      eventToPromise('password-denied-for-testing', dialog);
  submitPassword(dialog);
  await whenPasswordDenied;

  // The input field should be marked invalid and re-enabled.
  assertTrue(dialog.$.password.invalid);
  assertFalse(dialog.$.password.disabled);

  // The submit button should be re-enabled.
  assertFalse(dialog.$.submit.disabled);
}

async function tryCorrectPassword(
    dialog: ViewerPasswordDialogElement, password: string) {
  dialog.$.password.value = password;

  // The dialog should close in response to a correct password.
  const whenDialogClosed = eventToPromise('close', dialog);
  submitPassword(dialog);
  await whenDialogClosed;
  assertNull(getPasswordDialog());
}

suite('ViewerPasswordDialog', function() {
  test('PasswordDialog', async function() {
    const dialog = getPasswordDialog();
    assertTrue(!!dialog);

    // The input field should be marked valid and enabled.
    assertFalse(dialog.$.password.invalid);
    assertFalse(dialog.$.password.disabled);

    // The submit button should be enabled.
    assertFalse(dialog.$.submit.disabled);

    await tryIncorrectPassword(dialog, 'incorrect');
    await tryCorrectPassword(dialog, 'ownerpass');

    const toolbar = viewer.shadowRoot.querySelector('viewer-toolbar')!;
    assertFalse(toolbar.annotationAvailable);
  });
});
