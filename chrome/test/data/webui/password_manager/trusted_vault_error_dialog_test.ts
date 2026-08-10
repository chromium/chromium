// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {TrustedVaultErrorDialogElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

suite('TrustedVaultErrorDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let dialog: TrustedVaultErrorDialogElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    await flushTasks();

    dialog = document.createElement('trusted-vault-error-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
  });

  test('dialog opens on connectedCallback', function() {
    assertTrue(dialog.$.dialog.open);
  });

  test('clicking cancel closes the dialog', async function() {
    assertTrue(dialog.$.dialog.open);
    const cancelButton =
        dialog.shadowRoot!.querySelector<HTMLElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await flushTasks();
    assertFalse(dialog.$.dialog.open);
  });

  test('clicking verify calls startTrustedVaultUnlock', async function() {
    assertTrue(dialog.$.dialog.open);
    const verifyButton =
        dialog.shadowRoot!.querySelector<HTMLElement>('#verifyButton');
    assertTrue(!!verifyButton);
    verifyButton.click();
    await passwordManager.whenCalled('startTrustedVaultUnlock');
    await flushTasks();
    assertFalse(dialog.$.dialog.open);
  });

  test('displays dialog texts', function() {
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogTitle'),
        dialog.shadowRoot!.querySelector('.title-heading')!.textContent.trim());
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogDescription'),
        dialog.shadowRoot!.querySelector('.body-text')!.textContent.trim());
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogContinue'),
        dialog.shadowRoot!.querySelector('#verifyButton')!.textContent.trim());
    assertEquals(
        loadTimeData.getString('trustedVaultErrorDialogNotNow'),
        dialog.shadowRoot!.querySelector('#cancelButton')!.textContent.trim());
  });
});
