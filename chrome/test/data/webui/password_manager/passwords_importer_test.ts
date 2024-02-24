// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CrButtonElement, CrDialogElement, PasswordsImporterElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, PluralStringProxyImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';


function createPasswordsImporter(
    isUserSyncingPasswords: boolean = false,
    isAccountStoreUser: boolean = false,
    accountEmail: string = ''): PasswordsImporterElement {
  const passwordsImporter = document.createElement('passwords-importer');
  passwordsImporter.isUserSyncingPasswords = isUserSyncingPasswords;
  passwordsImporter.isAccountStoreUser = isAccountStoreUser;
  passwordsImporter.accountEmail = accountEmail;
  document.body.appendChild(passwordsImporter);
  flush();
  return passwordsImporter;
}

async function triggerImportHelper(
    importer: PasswordsImporterElement,
    passwordManager: TestPasswordManagerProxy,
    expectedStore: chrome.passwordsPrivate.PasswordStoreSet =
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE) {
  const chooseFile =
      importer.shadowRoot!.querySelector<HTMLElement>('#selectFileButton');
  assertTrue(!!chooseFile);
  assertTrue(isVisible(chooseFile));
  chooseFile.click();
  flush();

  // In progress state after the click.
  const spinner = importer.shadowRoot!.querySelector('paper-spinner-lite');
  assertTrue(!!spinner);
  assertTrue(spinner.active);
  assertFalse(isVisible(chooseFile));

  // Import flow should have been triggered.
  const destinationStore = await passwordManager.whenCalled('importPasswords');
  assertEquals(expectedStore, destinationStore);
}

function assertVisibleTextContent(
    parent: HTMLElement, selector: string, expectedText: string) {
  const element = parent.querySelector<HTMLElement>(selector);
  assertTrue(!!element);
  assertTrue(isVisible(element));
  assertEquals(expectedText, element?.textContent!.trim());
}

async function closeDialogHelper(
    importer: PasswordsImporterElement,
    passwordManager: TestPasswordManagerProxy, dialog: HTMLElement,
    selector: string, shouldDeleteFile: boolean = false) {
  const button = dialog.querySelector<HTMLElement>(selector);
  assertTrue(!!button);
  // Should close the dialog and trigger 'passwordsPrivate.resetImporter'.
  button.click();
  const deleteFile = await passwordManager.whenCalled('resetImporter');
  assertEquals(shouldDeleteFile, deleteFile);
  await flushTasks();
  assertFalse(!!importer.shadowRoot!.querySelector<CrDialogElement>('#dialog'));
}

async function assertErrorStateAndClose(
    importer: PasswordsImporterElement,
    passwordManager: TestPasswordManagerProxy, expectedDescription: string) {
  const dialog = importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
  assertTrue(!!dialog);
  assertTrue(dialog.open);

  assertVisibleTextContent(
      dialog, '#title', importer.i18n('importPasswordsErrorTitle'));

  const description = dialog.querySelector('#description');
  assertTrue(!!description);
  assertEquals(expectedDescription, description.innerHTML.toString());

  assertVisibleTextContent(
      dialog, '#selectFileButton', importer.i18n('selectFile'));
  assertVisibleTextContent(dialog, '#closeButton', importer.i18n('close'));

  await closeDialogHelper(importer, passwordManager, dialog, '#closeButton');
}

suite('PasswordsImporterTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
  });

  test('has correct non-syncing initial state', function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ false);

    const expectedDescription =
        importer.i18n('importPasswordsDescriptionDevice');
    assertEquals(expectedDescription, importer.$.linkRow.subLabel);
    assertTrue(importer.$.linkRow.hasAttribute('hide-icon'));
  });

  test('has correct syncing initial state', function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ true, /*isAccountStoreUser=*/ false,
        /*accountEmail=*/ 'test@test.com');

    const expectedDescription = importer.i18n(
        'importPasswordsDescriptionAccount',
        importer.i18n('localPasswordManager'), 'test@test.com');
    assertEquals(expectedDescription, importer.$.linkRow.subLabel);
    assertTrue(importer.$.linkRow.hasAttribute('hide-icon'));
  });

  test('has correct initial state for account store users', function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ true,
        /*accountEmail=*/ 'test@test.com');

    const expectedDescription =
        importer.i18n('importPasswordsGenericDescription');
    assertEquals(expectedDescription, importer.$.linkRow.subLabel);
    assertFalse(importer.$.linkRow.hasAttribute('hide-icon'));
  });

  test('can trigger import', async function() {
    const importer = createPasswordsImporter();

    await triggerImportHelper(importer, passwordManager);
  });

  test('store picker dialog has correct state', async function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ true,
        /*accountEmail=*/ 'test@test.com');

    // Clicking on the importer row should open the STORE_PICKER dialog.
    importer.$.linkRow.click();
    flush();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertTrue(isChildVisible(dialog, '#storePicker', /*checkLightDom=*/ true));

    assertVisibleTextContent(
        dialog, '#title', importer.i18n('importPasswords'));
    assertVisibleTextContent(
        dialog, '#description', importer.i18n('importPasswordsSelectFile'));
    assertVisibleTextContent(
        dialog, '#selectFileButton', importer.i18n('selectFile'));
    assertVisibleTextContent(dialog, '#cancelButton', importer.i18n('cancel'));

    await closeDialogHelper(importer, passwordManager, dialog, '#cancelButton');
  });

  test('account store user can import passwords to device', async function() {
    // Store picker should pre-select preferred store as DEVICE.
    passwordManager.data.isAccountStorageDefault = false;
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ true,
        /*accountEmail=*/ 'test@test.com');
    await flushTasks();

    // Clicking on the importer row should open the STORE_PICKER dialog.
    importer.$.linkRow.click();
    flush();

    const expectedStore = chrome.passwordsPrivate.PasswordStoreSet.DEVICE;
    await triggerImportHelper(importer, passwordManager, expectedStore);
  });

  test('account store user can import passwords to account', async function() {
    // Store picker should pre-select preferred store as ACCOUNT.
    passwordManager.data.isAccountStorageDefault = true;
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ true,
        /*accountEmail=*/ 'test@test.com');
    await flushTasks();

    // Clicking on the importer row should open the STORE_PICKER dialog.
    importer.$.linkRow.click();
    flush();

    const expectedStore = chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;
    await triggerImportHelper(importer, passwordManager, expectedStore);
  });

  test('Has correct success state with no errors', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
      numberImported: 42,
      displayedEntries: [],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertVisibleTextContent(
        dialog, '#title', importer.i18n('importPasswordsSuccessTitle'));

    assertTrue(
        isChildVisible(dialog, '#deleteFileOption', /*checkLightDom=*/ true));

    assertFalse(isChildVisible(dialog, '#tipBox', /*checkLightDom=*/ true));
    assertFalse(
        isChildVisible(dialog, '#failuresSummary', /*checkLightDom=*/ true));

    const deleteFileOption = dialog.querySelector('#deleteFileOption');
    assertTrue(!!deleteFileOption);
    assertEquals(
        deleteFileOption.innerHTML.toString(),
        importer
            .i18nAdvanced(
                'importPasswordsDeleteFileOption',
                {attrs: ['class'], substitutions: ['test.csv']})
            .toString());

    assertVisibleTextContent(dialog, '#closeButton', importer.i18n('close'));

    await closeDialogHelper(importer, passwordManager, dialog, '#closeButton');
  });

  test('has correct conflicts state', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.CONFLICTS,
      numberImported: 0,
      displayedEntries: [
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://google.com',
          password: 'pwd',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://test.com',
          password: 'pwd',
          id: 1,
        },
      ],
      fileName: 'test.csv',
    });
    const expectedTitle = '2 existing passwords found';
    pluralString.text = expectedTitle;

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertVisibleTextContent(dialog, '#title', expectedTitle);

    const passwordItems = dialog.querySelectorAll('password-preview-item');
    assertEquals(2, passwordItems.length);
    assertTrue(Array.from(passwordItems).every((el) => !el.checked));

    const replaceButton =
        dialog.querySelector<CrButtonElement>('#replaceButton');
    assertTrue(!!replaceButton);
    assertTrue(replaceButton.disabled);

    assertVisibleTextContent(
        dialog, '#cancelButton', importer.i18n('importPasswordsCancel'));
    assertVisibleTextContent(
        dialog, '#skipButton', importer.i18n('importPasswordsSkip'));
    assertVisibleTextContent(
        dialog, '#replaceButton', importer.i18n('importPasswordsReplace'));

    await closeDialogHelper(importer, passwordManager, dialog, '#cancelButton');
  });

  test('can skip conflicts', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.CONFLICTS,
      numberImported: 0,
      displayedEntries: [
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://google.com',
          password: 'pwd',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://test.com',
          password: 'pwd',
          id: 1,
        },
      ],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    const skipButton = dialog.querySelector<CrButtonElement>('#skipButton');
    assertTrue(!!skipButton);
    skipButton.click();
    flush();

    // In progress state after the click.
    const spinner = importer.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    assertFalse(
        !!importer.shadowRoot!.querySelector<CrDialogElement>('#dialog'));

    const selectedIds = await passwordManager.whenCalled('continueImport');
    assertEquals(0, selectedIds.length);
  });

  test('can continue import with conflicts', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.CONFLICTS,
      numberImported: 0,
      displayedEntries: [
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://google.com',
          password: 'pwd',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://test.com',
          password: 'pwd',
          id: 1,
        },
      ],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    const passwordItems = dialog.querySelectorAll('password-preview-item');
    for (const item of passwordItems) {
      item.$.checkbox.click();
      await item.$.checkbox.updateComplete;
    }

    const replaceButton =
        dialog.querySelector<CrButtonElement>('#replaceButton');
    assertTrue(!!replaceButton);
    replaceButton.click();
    flush();

    // In progress state after the click.
    const spinner = importer.shadowRoot!.querySelector('paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    assertFalse(
        !!importer.shadowRoot!.querySelector<CrDialogElement>('#dialog'));

    const selectedIds = await passwordManager.whenCalled('continueImport');
    assertArrayEquals([0, 1], selectedIds);
  });

  test('correct conflicts state after failed re-auth', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.CONFLICTS,
      numberImported: 0,
      displayedEntries: [
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://google.com',
          password: 'pwd',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.VALID,
          username: 'username',
          url: 'https://test.com',
          password: 'pwd',
          id: 1,
        },
      ],
      fileName: 'test.csv',
    });
    const expectedTitle = '2 existing passwords found';
    pluralString.text = expectedTitle;

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    let dialog = importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    let passwordItems = dialog.querySelectorAll('password-preview-item');
    // Select all rows.
    for (const item of passwordItems) {
      item.$.checkbox.click();
      await item.$.checkbox.updateComplete;
    }

    let replaceButton = dialog.querySelector<CrButtonElement>('#replaceButton');
    assertTrue(!!replaceButton);
    replaceButton.click();
    flush();

    assertFalse(
        !!importer.shadowRoot!.querySelector<CrDialogElement>('#dialog'));

    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.DISMISSED,
      numberImported: 0,
      displayedEntries: [],
      fileName: '',
    });
    const selectedIds = await passwordManager.whenCalled('continueImport');
    assertArrayEquals([0, 1], selectedIds);
    await flushTasks();
    dialog = importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertVisibleTextContent(dialog, '#title', expectedTitle);

    passwordItems = dialog.querySelectorAll('password-preview-item');
    assertTrue(Array.from(passwordItems).every((el) => el.checked));

    replaceButton = dialog.querySelector<CrButtonElement>('#replaceButton');
    assertTrue(!!replaceButton);
    assertFalse(replaceButton.disabled);
  });

  test(
      'close button triggers file deletion with ticked checkbox',
      async function() {
        const importer = createPasswordsImporter();
        passwordManager.setImportResults({
          status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
          numberImported: 42,
          displayedEntries: [],
          fileName: 'test.csv',
        });

        await triggerImportHelper(importer, passwordManager);
        await pluralString.whenCalled('getPluralString');
        await flushTasks();

        const dialog =
            importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        const deleteFileOption =
            dialog.querySelector<HTMLElement>('#deleteFileOption');
        assertTrue(!!deleteFileOption);
        deleteFileOption.click();
        flush();

        await closeDialogHelper(
            importer, passwordManager, dialog, '#closeButton',
            /*shouldDeleteFile=*/ true);
      });

  test(
      'view passwords triggers file deletion with ticked checkbox',
      async function() {
        const importer = createPasswordsImporter();
        passwordManager.setImportResults({
          status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
          numberImported: 42,
          displayedEntries: [],
          fileName: 'test.csv',
        });

        await triggerImportHelper(importer, passwordManager);
        await pluralString.whenCalled('getPluralString');
        await flushTasks();

        const dialog =
            importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
        assertTrue(!!dialog);
        assertTrue(dialog.open);

        const deleteFileOption =
            dialog.querySelector<HTMLElement>('#deleteFileOption');
        assertTrue(!!deleteFileOption);
        deleteFileOption.click();
        flush();

        await closeDialogHelper(
            importer, passwordManager, dialog, '#viewPasswordsButton',
            /*shouldDeleteFile=*/ true);
      });


  test('view passwords navigates to the passwords page', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
      numberImported: 42,
      displayedEntries: [],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertVisibleTextContent(
        dialog, '#viewPasswordsButton', importer.i18n('viewPasswordsButton'));
    // Should close the dialog and navigate to PASSWORDS page.
    await closeDialogHelper(
        importer, passwordManager, dialog, '#viewPasswordsButton');
    assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
  });

  test('has correct success state with failures', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.SUCCESS,
      numberImported: 42,
      displayedEntries: [
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.MISSING_PASSWORD,
          username: 'username',
          url: 'https://google.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.MISSING_URL,
          username: 'username',
          url: '',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.INVALID_URL,
          username: 'username',
          url: 'http/google.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.LONG_URL,
          username: 'username',
          url: 'https://morethan2048chars.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.LONG_PASSWORD,
          username: 'username',
          url: 'https://google.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.LONG_USERNAME,
          username: 'morethan1000chars',
          url: 'https://google.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_PROFILE,
          username: 'username',
          url: 'https://google.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.CONFLICT_ACCOUNT,
          username: 'username',
          url: 'https://google.com',
          password: '',
          id: 0,
        },
        {
          status: chrome.passwordsPrivate.ImportEntryStatus.UNKNOWN_ERROR,
          username: '',
          url: '',
          password: '',
          id: 0,
        },
      ],
      fileName: 'test.csv',
    });

    await triggerImportHelper(importer, passwordManager);
    await pluralString.whenCalled('getPluralString');
    await flushTasks();

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertVisibleTextContent(
        dialog, '#title', importer.i18n('importPasswordsCompleteTitle'));

    // Success tip should not be visible.
    assertFalse(isChildVisible(dialog, '#tipBox', /*checkLightDom=*/ true));
    assertFalse(
        isChildVisible(dialog, '#deleteFileOption', /*checkLightDom=*/ true));

    assertTrue(
        isChildVisible(dialog, '#failuresSummary', /*checkLightDom=*/ true));

    assertVisibleTextContent(dialog, '#closeButton', importer.i18n('close'));

    await closeDialogHelper(importer, passwordManager, dialog, '#closeButton');
  });

  test('bad format error dialog is correct', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.BAD_FORMAT,
      numberImported: 0,
      displayedEntries: [],
      fileName: 'test.csv',
    });
    await triggerImportHelper(importer, passwordManager);

    await assertErrorStateAndClose(
        importer, passwordManager,
        importer
            .i18nAdvanced(
                'importPasswordsBadFormatError',
                {
                  attrs: ['class'],
                  substitutions: [
                    'test.csv',
                    loadTimeData.getString('importPasswordsHelpURL'),
                  ],
                },
                )
            .toString());
  });

  test('unknown error error dialog is correct', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.IO_ERROR,
      numberImported: 0,
      displayedEntries: [],
      fileName: 'test.csv',
    });
    await triggerImportHelper(importer, passwordManager);

    await assertErrorStateAndClose(
        importer, passwordManager,
        importer.i18nAdvanced('importPasswordsUnknownError').toString());
  });

  test('passwords per file limit error dialog is correct', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status:
          chrome.passwordsPrivate.ImportResultsStatus.NUM_PASSWORDS_EXCEEDED,
      numberImported: 0,
      displayedEntries: [],
      fileName: 'test.csv',
    });
    await triggerImportHelper(importer, passwordManager);

    await assertErrorStateAndClose(
        importer, passwordManager,
        importer.i18nAdvanced('importPasswordsLimitExceeded').toString());
  });

  test('file size exceeded error dialog is correct', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.MAX_FILE_SIZE,
      numberImported: 0,
      displayedEntries: [],
      fileName: 'test.csv',
    });
    await triggerImportHelper(importer, passwordManager);

    await assertErrorStateAndClose(
        importer, passwordManager,
        importer.i18nAdvanced('importPasswordsFileSizeExceeded').toString());
  });

  test('already active dialog state has correct state', async function() {
    const importer = createPasswordsImporter();
    passwordManager.setImportResults({
      status: chrome.passwordsPrivate.ImportResultsStatus.IMPORT_ALREADY_ACTIVE,
      numberImported: 0,
      displayedEntries: [],
      fileName: '',
    });

    await triggerImportHelper(importer, passwordManager);

    const dialog =
        importer.shadowRoot!.querySelector<CrDialogElement>('#dialog');
    assertTrue(!!dialog);
    assertTrue(dialog.open);

    assertVisibleTextContent(
        dialog, '#title', importer.i18n('importPasswords'));
    assertVisibleTextContent(
        dialog, '#description', importer.i18n('importPasswordsAlreadyActive'));
    assertVisibleTextContent(dialog, '#closeButton', importer.i18n('close'));

    await closeDialogHelper(importer, passwordManager, dialog, '#closeButton');
  });
});
