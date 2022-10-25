// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Import Dialog tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {IMPORT_HELP_LANDING_PAGE, ImportDialogState, PasswordsImportDialogElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl, SettingsPluralStringProxyImpl, CrButtonElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

async function triggerImportHelper(
    importDialog: PasswordsImportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const chooseFile =
      importDialog.shadowRoot!.querySelector<CrButtonElement>('#chooseFile');
  assertTrue(!!chooseFile);
  assertTrue(isVisible(chooseFile));
  chooseFile.click();
  flush();

  // In progress state after the click.
  const spinner = importDialog.shadowRoot!.querySelector('paper-spinner-lite');
  assertTrue(!!spinner);
  assertTrue(spinner.active);
  assertTrue(chooseFile.disabled);

  // Import flow should have been triggered.
  await passwordManager.whenCalled('importPasswords');
}

function assertIntialStatePartsAndClose(
    importDialog: PasswordsImportDialogElement, expectedDescription: string) {
  assertEquals(ImportDialogState.START, importDialog.dialogState);

  const spinner = importDialog.shadowRoot!.querySelector('paper-spinner-lite');
  assertTrue(!!spinner);
  assertFalse(spinner.active);

  assertTrue(isVisible(importDialog.$.chooseFile));
  assertFalse(importDialog.$.close.disabled);
  assertFalse(importDialog.$.chooseFile.disabled);

  assertFalse(isVisible(
      importDialog.shadowRoot!.querySelector<HTMLElement>('#tipBox')));

  assertEquals(
      importDialog.i18n('cancel'), importDialog.$.close.textContent!.trim());
  assertEquals(
      importDialog.i18n('importPasswordsChooseFile'),
      importDialog.$.chooseFile.textContent!.trim());
  assertEquals(
      expectedDescription, importDialog.$.descriptionText.textContent!.trim());
  importDialog.$.close.click();
}

async function assertErrorStateAndClose(
    importDialog: PasswordsImportDialogElement,
    passwordManager: TestPasswordManagerProxy, expectedDescription: string) {
  await triggerImportHelper(importDialog, passwordManager);
  flush();
  // After the import, the dialog should switch to ERROR state.
  assertEquals(ImportDialogState.ERROR, importDialog.dialogState);

  assertEquals(
      expectedDescription, importDialog.$.descriptionText.innerHTML!.trim());

  assertFalse(isVisible(
      importDialog.shadowRoot!.querySelector<HTMLElement>('#tipBox')));

  assertEquals(
      importDialog.i18n('close'), importDialog.$.close.textContent!.trim());
  assertFalse(importDialog.$.close.disabled);
  importDialog.$.close.click();
  await eventToPromise('close', importDialog);
}

suite('PasswordsImportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;
  let elementFactory: PasswordSectionElementFactory;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    SettingsPluralStringProxyImpl.setInstance(pluralString);
    elementFactory = new PasswordSectionElementFactory(document);
  });

  test('hasCorrectNonSyncingInitialState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ false);

    assertIntialStatePartsAndClose(
        importDialog, importDialog.i18n('importPasswordsDescriptionDevice'));

    await eventToPromise('close', importDialog);
  });

  test('hasCorrectSyncingInitialState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog(
        /*isUserSyncingPasswords=*/ true, /*isAccountStoreUser=*/ false,
        /*accountEmail=*/ 'test@test.com');

    assertIntialStatePartsAndClose(
        importDialog,
        importDialog.i18n(
            'importPasswordsDescriptionAccount', 'test@test.com'));

    await eventToPromise('close', importDialog);
  });

  test('hasCorrectButterInitialState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ true,
        /*accountEmail=*/ 'test@test.com');

    assertIntialStatePartsAndClose(
        importDialog, importDialog.i18n('importPasswordsGenericDescription'));

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

    assertTrue(isVisible(
        importDialog.shadowRoot!.querySelector<HTMLElement>('#tipBox')));
    assertEquals(
        importDialog.i18nAdvanced('importPasswordsSuccessTip')
            .toString()
            .replace('<b></b>', 'test.csv'),
        importDialog.$.successTip.textContent!.trim());

    // Failed imports summary should not be visible.
    assertFalse(isVisible(importDialog.$.failuresSummary));

    assertEquals(
        importDialog.i18n('done'), importDialog.$.close.textContent!.trim());
    assertFalse(importDialog.$.close.disabled);
    // check console for exceptions!
    importDialog.$.close.click();
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

    await assertErrorStateAndClose(
        importDialog, passwordManager,
        importDialog
            .i18nAdvanced(
                'importPasswordsBadFormatError',
                {substitutions: [IMPORT_HELP_LANDING_PAGE]},
                )
            .toString()
            .replace('<b></b>', '<b>test.csv</b>'));
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

    await assertErrorStateAndClose(
        importDialog, passwordManager,
        importDialog.i18n('importPasswordsUnknownError'));
  });

  test('hasCorrectLimitExceededState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status:
          chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED,
      numberImported: 0,
      failedImports: [],
      fileName: 'test.csv',
    });

    await assertErrorStateAndClose(
        importDialog, passwordManager,
        importDialog.i18n('importPasswordsLimitExceeded', 3000));
  });

  test('hasCorrectFileSizeExceededState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE,
      numberImported: 0,
      failedImports: [],
      fileName: 'test.csv',
    });

    await assertErrorStateAndClose(
        importDialog, passwordManager,
        importDialog.i18n('importPasswordsFileSizeExceeded'));
  });

  test('hasCorrectSuccessStateWithFailures', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
      numberImported: 42,
      failedImports: [
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.MISSING_PASSWORD,
          username: 'username',
          url: 'https://google.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.MISSING_URL,
          username: 'username',
          url: '',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.INVALID_URL,
          username: 'username',
          url: 'http/google.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.LONG_URL,
          username: 'username',
          url: 'https://morethan2048chars.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.NON_ASCII_URL,
          username: 'username',
          url: 'https://أهلا.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.LONG_PASSWORD,
          username: 'username',
          url: 'https://google.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.LONG_USERNAME,
          username: 'morethan1000chars',
          url: 'https://google.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_PROFILE,
          username: 'username',
          url: 'https://google.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_ACCOUNT,
          username: 'username',
          url: 'https://google.com',
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR,
          username: '',
          url: '',
        },
      ],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importDialog, passwordManager);
    await pluralString.whenCalled('getPluralString');
    flush();
    // After the import, the dialog should switch to SUCCESS state.
    assertEquals(ImportDialogState.SUCCESS, importDialog.dialogState);

    // Success tip should not be visible.
    assertFalse(isVisible(
        importDialog.shadowRoot!.querySelector<HTMLElement>('#tipBox')));

    assertTrue(isVisible(importDialog.$.failuresSummary));
    assertEquals(
        importDialog.i18n('importPasswordsFailuresSummary', 10),
        importDialog.$.failuresSummary.textContent!.trim());

    assertEquals(
        importDialog.i18n('done'), importDialog.$.close.textContent!.trim());
    assertFalse(importDialog.$.close.disabled);
    importDialog.$.close.click();
    await eventToPromise('close', importDialog);
  });

  test('hasCorrectAlreadyActiveState', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertEquals(ImportDialogState.START, importDialog.dialogState);
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.IMPORT_ALREADY_ACTIVE,
      numberImported: 0,
      failedImports: [],
      fileName: '',
    });

    await triggerImportHelper(importDialog, passwordManager);
    flush();
    // After the import, the dialog should switch to ALREADY_ACTIVE state.
    assertEquals(ImportDialogState.ALREADY_ACTIVE, importDialog.dialogState);

    const infoIcon =
        importDialog.shadowRoot!.querySelector<HTMLElement>('#infoIcon');
    assertTrue(!!infoIcon);
    assertTrue(isVisible(infoIcon));

    assertEquals(
        importDialog.i18n('importPasswordsAlreadyActive'),
        importDialog.$.descriptionText.textContent!.trim());

    assertFalse(isVisible(
        importDialog.shadowRoot!.querySelector<HTMLElement>('#tipBox')));


    assertEquals(
        importDialog.i18n('close'), importDialog.$.close.textContent!.trim());
    assertFalse(importDialog.$.close.disabled);
    importDialog.$.close.click();
    await eventToPromise('close', importDialog);
  });
});
