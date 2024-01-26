// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsSimpleConfirmationDialogElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('settings-simple-confirmation-dialog', function() {
  let dialog: SettingsSimpleConfirmationDialogElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-simple-confirmation-dialog');
    document.body.appendChild(dialog);
  });

  test('strings', function() {
    dialog.titleText = 'fooTitle';
    dialog.bodyText = 'fooBody';
    dialog.confirmText = 'fooConfirm';

    const title = dialog.shadowRoot!.querySelector('[slot=title]');
    assertTrue(!!title);
    assertEquals(dialog.titleText, title.textContent);

    const body = dialog.shadowRoot!.querySelector('[slot=body]');
    assertTrue(!!body);
    assertEquals(dialog.bodyText, body.textContent!.trim());

    assertEquals(dialog.confirmText, dialog.$.confirm.textContent!.trim());
  });

  test('noPrimaryButton', function() {
    assertFalse(dialog.noPrimaryButton);
    assertTrue(dialog.$.confirm.classList.contains('action-button'));

    dialog.noPrimaryButton = true;
    assertFalse(dialog.$.confirm.classList.contains('action-button'));
  });

  test('wasConfirmed_false', async function() {
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.cancel.click();
    await whenClosed;
    assertFalse(dialog.wasConfirmed());
  });

  test('wasConfirmed_true', async function() {
    const whenClosed = eventToPromise('close', dialog);
    dialog.$.confirm.click();
    await whenClosed;
    assertTrue(dialog.wasConfirmed());
  });
});
