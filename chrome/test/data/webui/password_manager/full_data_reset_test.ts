// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

suite('FullDataResetTest', function() {
  let passwordManager: TestPasswordManagerProxy;


  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    passwordManager.data.deleteAllPasswordManagerData = true;
  });

  test('confirmation dialog is closed by default', function() {
    const dataReset = document.createElement('full-data-reset');
    document.body.appendChild(dataReset);

    assertFalse(dataReset.$.dialog.open);
  });

  test(
      'getSavedPasswordList is called on deleteAllButton click',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        document.body.appendChild(dataReset);

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        await passwordManager.whenCalled('getSavedPasswordList');
      });

  test(
      'confirmation dialog is closed by clicking the cancel button',
      function() {
        const dataReset = document.createElement('full-data-reset');
        document.body.appendChild(dataReset);

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        assertTrue(dataReset.$.dialog.open);

        // Should close the dialog.
        assertTrue(isVisible(dataReset.$.cancelButton));
        dataReset.$.cancelButton.click();

        assertFalse(dataReset.$.dialog.open);
      });

  test(
      'confirmation dialog is closed by clicking the action button',
      async function() {
        const dataReset = document.createElement('full-data-reset');
        document.body.appendChild(dataReset);

        // Open the dialog.
        dataReset.$.deleteAllButton.click();
        assertTrue(dataReset.$.dialog.open);

        // Should close the dialog and trigger passwordsPrivate call.
        assertTrue(isVisible(dataReset.$.confirmButton));
        dataReset.$.confirmButton.click();

        await passwordManager.whenCalled('deleteAllPasswordManagerData');
        await microtasksFinished();

        assertTrue(dataReset.$.successToast.open);
        assertFalse(dataReset.$.dialog.open);
      });

  test(
      'confirmation dialog has correct state for local users', function() {
        const dataReset = document.createElement('full-data-reset');
        dataReset.isSyncingPasswords = false;
        document.body.appendChild(dataReset);

        // Open the dialog.
        dataReset.$.deleteAllButton.click();

        assertEquals(
            dataReset.$.confirmationDialogTitle.textContent?.trim(),
            dataReset.i18n('fullResetConfirmationTitleLocal'));
      });

  test(
      'confirmation dialog has correct state for syncing users', function() {
        const dataReset = document.createElement('full-data-reset');
        dataReset.isSyncingPasswords = true;
        document.body.appendChild(dataReset);

        // Open the dialog.
        dataReset.$.deleteAllButton.click();

        assertEquals(
            dataReset.$.confirmationDialogTitle.textContent?.trim(),
            dataReset.i18n('fullResetConfirmationTitle'));
      });
});
