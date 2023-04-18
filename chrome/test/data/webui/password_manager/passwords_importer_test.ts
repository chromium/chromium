// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CrDialogElement, PasswordManagerImpl, PasswordsImporterElement} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
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

suite('PasswordsImporterTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
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

  test('store picker dialog has correct state', function() {
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
});
