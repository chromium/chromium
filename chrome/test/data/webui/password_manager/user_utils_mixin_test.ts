// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerActionableError, PasswordManagerImpl, SyncBrowserProxyImpl, TrustedVaultUnlockDialogSource, UserUtilMixin} from 'chrome://password-manager/password_manager.js';
import type {UserUtilMixinInterface} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

const TestDummyElementBase = UserUtilMixin(CrLitElement);

interface TestDummyElement extends UserUtilMixinInterface {}

class TestDummyElement extends TestDummyElementBase {
  static get is() {
    return 'test-dummy';
  }
}

customElements.define(TestDummyElement.is, TestDummyElement);

suite('UserUtilMixinTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;
  let element: TestDummyElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);

    element = document.createElement('test-dummy') as TestDummyElement;
    document.body.appendChild(element);
    await microtasksFinished();
  });

  test('initial actionableError is set', async function() {
    passwordManager.data.getActionableError =
        PasswordManagerActionableError.kTrustedVaultKeyNeeded;
    element = document.createElement('test-dummy') as TestDummyElement;
    document.body.appendChild(element);
    await microtasksFinished();
    assertEquals(
        PasswordManagerActionableError.kTrustedVaultKeyNeeded,
        element.actionableError);
  });

  test('actionableError updates when changed', async function() {
    assertEquals(
        PasswordManagerActionableError.kNoError, element.actionableError);

    passwordManager.listeners.passwordManagerActionableErrorChangedListener!
        (chrome.passwordsPrivate.PasswordManagerActionableError
             .TRUSTED_VAULT_KEY_NEEDED);
    await microtasksFinished();

    assertEquals(
        PasswordManagerActionableError.kTrustedVaultKeyNeeded,
        element.actionableError);

    passwordManager.listeners.passwordManagerActionableErrorChangedListener!
        (chrome.passwordsPrivate.PasswordManagerActionableError.NO_ERROR);
    await microtasksFinished();

    assertEquals(
        PasswordManagerActionableError.kNoError, element.actionableError);
  });

  test(
      'executeIfTrustedVaultUnlocked triggers dialog if vault is locked',
      async function() {
        loadTimeData.overrideValues({enableTrustedVaultUnlock: true});
        passwordManager.listeners.passwordManagerActionableErrorChangedListener!
            (chrome.passwordsPrivate.PasswordManagerActionableError
                 .TRUSTED_VAULT_KEY_NEEDED);
        await microtasksFinished();

        let dialogSource: TrustedVaultUnlockDialogSource|null = null;
        element.addEventListener('show-trusted-vault-error-dialog', e => {
          dialogSource = e.detail.source;
        });

        let actionExecuted = false;
        element.executeIfTrustedVaultUnlocked(() => {
          actionExecuted = true;
        }, TrustedVaultUnlockDialogSource.IMPORT);

        assertEquals(TrustedVaultUnlockDialogSource.IMPORT, dialogSource);
        assertFalse(actionExecuted);
      });

  test(
      'executeIfTrustedVaultUnlocked executes action if vault is not locked',
      async function() {
        loadTimeData.overrideValues({enableTrustedVaultUnlock: true});
        passwordManager.listeners.passwordManagerActionableErrorChangedListener!
            (chrome.passwordsPrivate.PasswordManagerActionableError.NO_ERROR);
        await microtasksFinished();

        let dialogFired = false;
        element.addEventListener('show-trusted-vault-error-dialog', () => {
          dialogFired = true;
        });

        let actionExecuted = false;
        element.executeIfTrustedVaultUnlocked(() => {
          actionExecuted = true;
        }, TrustedVaultUnlockDialogSource.ADD_PASSWORD);

        assertFalse(dialogFired);
        assertTrue(actionExecuted);
      });
});
