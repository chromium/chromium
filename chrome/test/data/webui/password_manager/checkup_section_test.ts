// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordCheckInteraction, PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {PluralStringProxy, PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeInsecureCredential, makePasswordCheckStatus} from './test_util.js';

const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

class CheckupTestPluralStringProxy extends TestBrowserProxy implements
    PluralStringProxy {
  constructor() {
    super([
      'checkedPasswords',
      'compromisedPasswords',
      'reusedPasswords',
      'weakPasswords',
    ]);
  }

  getPluralString(messageName: string, itemCount: number) {
    this.methodCalled(messageName, itemCount);
    return Promise.resolve('some text');
  }

  getPluralStringTupleWithComma(
      _messageName1: string, _itemCount1: number, _messageName2: string,
      _itemCount2: number) {
    return Promise.resolve('some text');
  }

  getPluralStringTupleWithPeriods(
      _messageName1: string, _itemCount1: number, _messageName2: string,
      _itemCount2: number) {
    return Promise.resolve('some text');
  }
}

suite('SettingsSectionTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let passwordManager: TestPasswordManagerProxy;
  let pluralString: CheckupTestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new CheckupTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
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

  test('Number of issues reflected in sections', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus(PasswordCheckState.IDLE);

    // 3 compromised, 0 reused, 4 weak credentials
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        types: [
          CompromiseType.PHISHED,
          CompromiseType.LEAKED,
          CompromiseType.WEAK,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.PHISHED,
          CompromiseType.WEAK,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.LEAKED,
          CompromiseType.WEAK,
        ],
      }),
      makeInsecureCredential({types: [CompromiseType.WEAK]}),
    ];

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await passwordManager.whenCalled('getPasswordCheckStatus');

    // Expect a proper number of insecure credentials as a parameter to
    // PluralStringProxy.
    assertEquals(3, await pluralString.whenCalled('compromisedPasswords'));
    assertEquals(0, await pluralString.whenCalled('reusedPasswords'));
    assertEquals(4, await pluralString.whenCalled('weakPasswords'));
    await flushTasks();

    // Expect a proper attribute for front icon color
    assertTrue(section.$.compromisedRow.hasAttribute('compromised'));
    assertFalse(section.$.reusedRow.hasAttribute('has-issues'));
    assertTrue(section.$.weakRow.hasAttribute('has-issues'));

    // Expect a proper rear icon state
    assertFalse(section.$.compromisedRow.hasAttribute('hide-icon'));
    assertTrue(section.$.reusedRow.hasAttribute('hide-icon'));
    assertFalse(section.$.weakRow.hasAttribute('hide-icon'));
  });
});
