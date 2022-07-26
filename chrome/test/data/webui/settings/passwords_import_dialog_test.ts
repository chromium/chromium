// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Import Dialog tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {ImportDialogState} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, flushTasks, isVisible} from 'chrome://webui-test/test_util.js';

import {PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

suite('PasswordsImportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;
  let elementFactory: PasswordSectionElementFactory;

  setup(function() {
    document.body.innerHTML = '';
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    elementFactory = new PasswordSectionElementFactory(document);
  });

  test('hasCorrectInitialState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(importDialog.dialogState, ImportDialogState.START);

    const cancel =
        importDialog.shadowRoot!.querySelector<HTMLElement>('#cancel');
    assertTrue(!!cancel);
    const chooseFile =
        importDialog.shadowRoot!.querySelector<HTMLElement>('#chooseFile');
    assertTrue(!!chooseFile);

    assertTrue(isVisible(cancel));
    assertTrue(isVisible(chooseFile));

    assertEquals(importDialog.i18n('cancel'), cancel.textContent!.trim());
    assertEquals(
        importDialog.i18n('importPasswordsChooseFile'),
        chooseFile.textContent!.trim());
    assertEquals(
        importDialog.i18n('importPasswordsGenericDescription'),
        importDialog.$.descriptionText.textContent!.trim());

    cancel.click();
    await eventToPromise('close', importDialog);
  });

  test('hasCorrectSuccessState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(importDialog.dialogState, ImportDialogState.START);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
      numberImported: 42,
      failedImports: [],
      fileName: 'test.csv',
    });

    const chooseFile =
        importDialog.shadowRoot!.querySelector<HTMLElement>('#chooseFile');
    assertTrue(!!chooseFile);
    assertTrue(isVisible(chooseFile));
    chooseFile.click();
    // Import flow should have been triggered.
    await passwordManager.whenCalled('importPasswords');
    await flushTasks();
    // After the import, the dialog should switch to SUCCESS state.
    assertEquals(importDialog.dialogState, ImportDialogState.SUCCESS);
    assertFalse(isVisible(chooseFile));

    const expectedSuccessSummary =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'importPasswordsSuccessSummaryDevice', 42);
    const successSummary = importDialog.$.descriptionText.textContent!.trim();
    assertEquals(expectedSuccessSummary, successSummary);

    const successTip =
        importDialog.shadowRoot!.querySelector<HTMLElement>('#successTip');
    assertTrue(!!successTip);
    assertEquals(
        importDialog.i18n('importPasswordsSuccessTip', 'test.csv'),
        successTip.textContent!.trim());

    const done = importDialog.shadowRoot!.querySelector<HTMLElement>('#done');
    assertTrue(!!done);
    assertEquals(importDialog.i18n('done'), done.textContent!.trim());
    assertTrue(isVisible(done));
    done.click();
    await eventToPromise('close', importDialog);
  });
});
