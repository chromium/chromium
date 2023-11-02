// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordCheckInteraction, PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makePasswordCheckStatus} from './test_util.js';

const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

suite('SettingsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML =
        window.trustedTypes!.emptyHTML as unknown as string;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('IDLE state', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.IDLE);

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    assertTrue(isVisible(section.$.checkupResult));
    assertTrue(isVisible(section.$.refreshButton));
    assertFalse(section.$.refreshButton.disabled);
    assertTrue(isVisible(section.$.lastCheckupTime));
    assertFalse(isVisible(section.$.retryButton));
    assertFalse(isVisible(section.$.spinner));
  });

  test('Running state', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.RUNNING);

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    assertFalse(isVisible(section.$.checkupResult));
    assertTrue(isVisible(section.$.refreshButton));
    assertTrue(section.$.refreshButton.disabled);
    assertFalse(isVisible(section.$.lastCheckupTime));
    assertFalse(isVisible(section.$.retryButton));
    assertTrue(isVisible(section.$.spinner));
  });

  [PasswordCheckState.NO_PASSWORDS, PasswordCheckState.QUOTA_LIMIT].forEach(
      state =>
          test(`State whcih prevents running check ${state}`, async function() {
            passwordManager.data.checkStatus = makePasswordCheckStatus(state);

            const section = document.createElement('checkup-section');
            document.body.appendChild(section);
            await flushTasks();

            assertTrue(isVisible(section.$.checkupResult));
            assertFalse(isVisible(section.$.refreshButton));
            assertFalse(isVisible(section.$.lastCheckupTime));
            assertFalse(isVisible(section.$.retryButton));
            assertFalse(isVisible(section.$.spinner));
          }));

  [PasswordCheckState.CANCELED, PasswordCheckState.OFFLINE,
   PasswordCheckState.SIGNED_OUT, PasswordCheckState.OTHER_ERROR]
      .forEach(state => test(`Error state ${state}`, async function() {
                 passwordManager.data.checkStatus =
                     makePasswordCheckStatus(state);

                 const section = document.createElement('checkup-section');
                 document.body.appendChild(section);
                 await flushTasks();

                 assertTrue(isVisible(section.$.checkupResult));
                 assertFalse(isVisible(section.$.refreshButton));
                 assertFalse(isVisible(section.$.lastCheckupTime));
                 assertTrue(isVisible(section.$.retryButton));
                 assertFalse(isVisible(section.$.spinner));
               }));

  test('Start check', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.IDLE);

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    section.$.refreshButton.click();
    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(PasswordCheckInteraction.START_CHECK_MANUALLY, interaction);
  });
});
