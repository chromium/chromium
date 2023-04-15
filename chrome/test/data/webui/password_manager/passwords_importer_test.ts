// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, PasswordsImporterElement} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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
    passwordManager: TestPasswordManagerProxy) {
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
  await passwordManager.whenCalled('importPasswords');
}

suite('PasswordsImporterTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('hasCorrectNonSyncingInitialState', function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ false);

    const expectedDescription =
        importer.i18n('importPasswordsDescriptionDevice');
    assertEquals(expectedDescription, importer.$.linkRow.subLabel);
    assertTrue(importer.$.linkRow.hasAttribute('hide-icon'));
  });

  test('hasCorrectSyncingInitialState', function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ true, /*isAccountStoreUser=*/ false,
        /*accountEmail=*/ 'test@test.com');


    const linkRow = importer.shadowRoot!.querySelector('cr-link-row');
    const expectedDescription = importer.i18n(
        'importPasswordsDescriptionAccount',
        importer.i18n('localPasswordManager'), 'test@test.com');
    assertEquals(expectedDescription, linkRow!.subLabel);
    assertTrue(importer.$.linkRow.hasAttribute('hide-icon'));
  });

  test('hasCorrectButterInitialState', function() {
    const importer = createPasswordsImporter(
        /*isUserSyncingPasswords=*/ false, /*isAccountStoreUser=*/ true,
        /*accountEmail=*/ 'test@test.com');

    const linkRow = importer.shadowRoot!.querySelector('cr-link-row');
    const expectedDescription =
        importer.i18n('importPasswordsGenericDescription');
    assertEquals(expectedDescription, linkRow!.subLabel);
    assertFalse(importer.$.linkRow.hasAttribute('hide-icon'));
  });

  test('canTriggerImport', async function() {
    const importer = createPasswordsImporter();

    await triggerImportHelper(importer, passwordManager);
  });
});
