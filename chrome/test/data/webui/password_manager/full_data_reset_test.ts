// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

suite('FullDataResetTest', function() {
  let passwordManager: TestPasswordManagerProxy;


  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    passwordManager.data.deleteAllPasswordManagerData = true;
  });

  test('confirmation dialog is closed by default', async function() {
    const dataReset = document.createElement('full-data-reset');
    document.body.appendChild(dataReset);
    flush();

    assertFalse(dataReset.$.dialog.open);
  });

  test(
      'getSavedPasswordList is called on deleteAllButton click',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        document.body.appendChild(dataReset);
        flush();

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        await passwordManager.whenCalled('getSavedPasswordList');
      });

  test(
      'confirmation dialog is closed by clicking the cancel button',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        document.body.appendChild(dataReset);
        flush();

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        flush();
        assertTrue(dataReset.$.dialog.open);

        // Should close the dialog.
        assertTrue(isVisible(dataReset.$.cancelButton));
        dataReset.$.cancelButton.click();
        flush();

        assertFalse(dataReset.$.dialog.open);
      });

  test(
      'confirmation dialog is closed by clicking the action button',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        document.body.appendChild(dataReset);
        flush();

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        flush();
        assertTrue(dataReset.$.dialog.open);

        // Should close the dialog and trigger passwordsPrivate call.
        assertTrue(isVisible(dataReset.$.confirmButton));
        dataReset.$.confirmButton.click();

        await passwordManager.whenCalled('deleteAllPasswordManagerData');
        flush();

        assertTrue(dataReset.$.successToast.open);
        assertFalse(dataReset.$.dialog.open);
      });

  test(
      'confirmation dialog has correct state for local users',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        dataReset.isSyncingPasswords = false;
        document.body.appendChild(dataReset);
        flush();

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        flush();

        assertEquals(
            dataReset.$.confirmationDialogTitle.textContent?.trim(),
            dataReset.i18n('fullResetConfirmationTitleLocal'));
      });

  test(
      'confirmation dialog has correct state for syncing users',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        dataReset.isSyncingPasswords = true;
        document.body.appendChild(dataReset);
        flush();

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        flush();

        assertEquals(
            dataReset.$.confirmationDialogTitle.textContent?.trim(),
            dataReset.i18n('fullResetConfirmationTitle'));
      });
});
