// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsGlicLoginPermissionsPageElement, SettingsSimpleConfirmationDialogElement} from 'chrome://settings/lazy_load.js';
import {assertEquals, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('GlicLoginPermissionsPage', function() {
  let page: SettingsGlicLoginPermissionsPageElement;

  const origin = 'example.com';

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-glic-login-permissions-page');
    page.set('actorLoginPermissions_', [{url: origin, username: 'user'}]);
    document.body.appendChild(page);
    await flushTasks();
  });

  test('login permissions list is visible', () => {
    const loginPermissionsList =
        page.shadowRoot!.querySelector('#actorLoginPermissionsList');
    assertTrue(!!loginPermissionsList);
    assertEquals(
        1, page.shadowRoot!.querySelectorAll('.permission-item').length);
  });

  test('remove dialog is shown', async () => {
    // Check that the remove dialog is not shown.
    assertNull(
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog'));

    // Click the remove button.
    const removeButton =
        page.shadowRoot!.querySelector<HTMLElement>('.icon-clear');
    assertTrue(!!removeButton);
    removeButton.click();
    await flushTasks();

    // Check that the remove dialog is shown.
    const dialog =
        page.shadowRoot!.querySelector<SettingsSimpleConfirmationDialogElement>(
            'settings-simple-confirmation-dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.bodyText.includes(origin));

    // Close the dialog.
    const closePromise = eventToPromise('close', dialog);
    dialog.$.cancel.click();
    await closePromise;
    await flushTasks();

    // Check that the remove dialog is not shown.
    assertNull(
        page.shadowRoot!.querySelector('settings-simple-confirmation-dialog'));
  });
});
