// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {RemoveActorLoginPermissionDialogElement} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('RemoveActorLoginPermissionDialogTest', function() {
  let dialog: RemoveActorLoginPermissionDialogElement;

  const ORIGIN = 'example.com';

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('remove-actor-login-permission-dialog');
    dialog.origin = ORIGIN;
    document.body.appendChild(dialog);
    flush();
  });

  test('dialog is visible', function() {
    assertTrue(dialog.$.dialog.open);
  });

  test('correct origin displayed', function() {
    const text = dialog.$.text.innerText;
    assertTrue(text.includes(ORIGIN));
  });

  test('disconnect button fires event and closes dialog', async function() {
    const eventPromise =
        eventToPromise('remove-actor-login-permission-click', dialog);
    dialog.$.disconnect.click();
    await eventPromise;
    assertFalse(dialog.$.dialog.open);
  });

  test('cancel button closes dialog', function() {
    dialog.shadowRoot!.querySelector<HTMLElement>('#cancel')!.click();
    assertFalse(dialog.$.dialog.open);
  });
});
