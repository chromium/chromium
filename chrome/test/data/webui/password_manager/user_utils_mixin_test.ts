// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerActionableError, PasswordManagerImpl, SyncBrowserProxyImpl, UserUtilMixin} from 'chrome://password-manager/password_manager.js';
import type {UserUtilMixinInterface} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';

const TestElementBase = UserUtilMixin(PolymerElement);

interface TestElement extends UserUtilMixinInterface {}

class TestElement extends TestElementBase {
  static get is() {
    return 'test-element';
  }
}

customElements.define(TestElement.is, TestElement);

suite('UserUtilMixinTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;
  let element: TestElement;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);

    element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);
    await flushTasks();
  });

  test('initial actionableError is set', async function() {
    passwordManager.data.getActionableError =
        PasswordManagerActionableError.kTrustedVaultKeyNeeded;
    element = document.createElement('test-element') as TestElement;
    document.body.appendChild(element);
    await flushTasks();
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
    await flushTasks();

    assertEquals(
        PasswordManagerActionableError.kTrustedVaultKeyNeeded,
        element.actionableError);

    passwordManager.listeners.passwordManagerActionableErrorChangedListener!
        (chrome.passwordsPrivate.PasswordManagerActionableError.NO_ERROR);
    await flushTasks();

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
        await flushTasks();

        let dialogFired = false;
        element.addEventListener('show-trusted-vault-error-dialog', () => {
          dialogFired = true;
        });

        let actionExecuted = false;
        element.executeIfTrustedVaultUnlocked(() => {
          actionExecuted = true;
        });

        assertTrue(dialogFired);
        assertFalse(actionExecuted);
      });

  test(
      'executeIfTrustedVaultUnlocked executes action if vault is not locked',
      async function() {
        loadTimeData.overrideValues({enableTrustedVaultUnlock: true});
        passwordManager.listeners.passwordManagerActionableErrorChangedListener!
            (chrome.passwordsPrivate.PasswordManagerActionableError.NO_ERROR);
        await flushTasks();

        let dialogFired = false;
        element.addEventListener('show-trusted-vault-error-dialog', () => {
          dialogFired = true;
        });

        let actionExecuted = false;
        element.executeIfTrustedVaultUnlocked(() => {
          actionExecuted = true;
        });

        assertFalse(dialogFired);
        assertTrue(actionExecuted);
      });
});
