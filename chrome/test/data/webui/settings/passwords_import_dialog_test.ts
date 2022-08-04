// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Import Dialog tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {ImportDialogState, PasswordsImportDialogElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl, SettingsPluralStringProxyImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

async function triggerImportHelper(
    importDialog: PasswordsImportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const chooseFile =
      importDialog.shadowRoot!.querySelector<HTMLElement>('#chooseFile');
  assertTrue(!!chooseFile);
  assertTrue(isVisible(chooseFile));
  chooseFile.click();
  // Import flow should have been triggered.
  await passwordManager.whenCalled('importPasswords');
}

suite('PasswordsImportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;
  let elementFactory: PasswordSectionElementFactory;

  setup(function() {
    document.body.innerHTML = '';
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);
    elementFactory = new PasswordSectionElementFactory(document);
  });

  test('hasCorrectInitialState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);

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
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
      numberImported: 42,
      failedImports: [],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importDialog, passwordManager);
    await pluralString.whenCalled('getPluralString');
    flush();
    // After the import, the dialog should switch to SUCCESS state.
    assertEquals(ImportDialogState.SUCCESS, importDialog.dialogState);

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

  test('hasCorrectFileErrorState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT,
      numberImported: 0,
      failedImports: [],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importDialog, passwordManager);
    flush();
    // After the import, the dialog should switch to ERROR state.
    assertEquals(ImportDialogState.ERROR, importDialog.dialogState);

    assertEquals(
        importDialog.i18n('importPasswordsBadFormatError', 'test.csv'),
        importDialog.$.descriptionText.textContent!.trim());

    const close = importDialog.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!close);
    assertEquals(importDialog.i18n('close'), close.textContent!.trim());
    assertTrue(isVisible(close));
    close.click();
    await eventToPromise('close', importDialog);
  });

  test('hasCorrectUnknownErrorState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR,
      numberImported: 0,
      failedImports: [],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importDialog, passwordManager);
    flush();
    // After the import, the dialog should switch to ERROR state.
    assertEquals(ImportDialogState.ERROR, importDialog.dialogState);

    assertEquals(
        importDialog.i18n('importPasswordsUnknownError'),
        importDialog.$.descriptionText.textContent!.trim());

    const close = importDialog.shadowRoot!.querySelector<HTMLElement>('#close');
    assertTrue(!!close);
    assertEquals(importDialog.i18n('close'), close.textContent!.trim());
    assertTrue(isVisible(close));
    close.click();
    await eventToPromise('close', importDialog);
  });
});
