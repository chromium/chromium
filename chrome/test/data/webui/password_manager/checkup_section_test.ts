// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CheckupSubpage, Page, PasswordCheckInteraction, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {PluralStringProxy, PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeInsecureCredential, makePasswordCheckStatus} from './test_util.js';

const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

// This is a special implementation of TestPluralStringProxy. It allows to await
// a call to |getPluralString| with a specific |messageName| parameter. The list
// of possible |messageNames| is passed to the constructor of TestBrowserProxy.
// This simplifies tests and allows to await for a |getPluralString| call with
// (checkedPasswords e.g.) and get the number being passed.
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
    return Promise.resolve(messageName);
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

suite('CheckupSectionTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let passwordManager: TestPasswordManagerProxy;
  let pluralString: CheckupTestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new CheckupTestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
    Router.getInstance().navigateTo(Page.CHECKUP);
    return flushTasks();
  });

  test('IDLE state', async function() {
    const elapsedTime = 'Two days ago.';
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        {state: PasswordCheckState.IDLE, lastCheck: elapsedTime});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    assertTrue(isVisible(section.$.checkupResult));
    assertTrue(isVisible(section.$.refreshButton));
    assertFalse(section.$.refreshButton.disabled);
    assertTrue(isVisible(section.$.lastCheckupTime));
    assertEquals(elapsedTime, section.$.lastCheckupTime.textContent!.trim());
    assertFalse(isVisible(section.$.retryButton));
    assertFalse(isVisible(section.$.spinner));
  });

  test('Running state', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus({state: PasswordCheckState.RUNNING});

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
            passwordManager.data.checkStatus =
                makePasswordCheckStatus({state: state});

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
                     makePasswordCheckStatus({state: state});

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
        makePasswordCheckStatus({state: PasswordCheckState.IDLE});

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
        makePasswordCheckStatus({state: PasswordCheckState.IDLE});

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

    // Expect string returned by PluralStringProxy.
    assertEquals('compromisedPasswords', section.$.compromisedRow.label);
    assertEquals('reusedPasswords', section.$.reusedRow.label);
    assertEquals('weakPasswords', section.$.weakRow.label);

    // Expect a proper attribute for front icon color
    assertTrue(section.$.compromisedRow.hasAttribute('compromised'));
    assertFalse(section.$.reusedRow.hasAttribute('has-issues'));
    assertTrue(section.$.weakRow.hasAttribute('has-issues'));

    // Expect a proper rear icon state
    assertFalse(section.$.compromisedRow.hasAttribute('hide-icon'));
    assertTrue(section.$.reusedRow.hasAttribute('hide-icon'));
    assertFalse(section.$.weakRow.hasAttribute('hide-icon'));
  });

  test('Number of checked passwords', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        {state: PasswordCheckState.IDLE, totalNumber: 10});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    assertEquals(10, await pluralString.whenCalled('checkedPasswords'));
    const statusLabel =
        section.shadowRoot!.querySelector<HTMLElement>('#checkupStatusLabel');
    assertTrue(!!statusLabel);
    assertEquals('checkedPasswords', statusLabel.textContent!.trim());
  });

  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(
              `clicking ${type} row navigates to details page`,
              async function() {
                passwordManager.data.checkStatus =
                    makePasswordCheckStatus({state: PasswordCheckState.IDLE});

                passwordManager.data.insecureCredentials =
                    [makeInsecureCredential({
                      types: [
                        CompromiseType.LEAKED,
                        CompromiseType.REUSED,
                        CompromiseType.WEAK,
                      ],
                    })];

                const section = document.createElement('checkup-section');
                document.body.appendChild(section);
                await passwordManager.whenCalled('getInsecureCredentials');
                await passwordManager.whenCalled('getPasswordCheckStatus');

                const listRow = section.shadowRoot!.querySelector<HTMLElement>(
                    `#${type}Row`);
                assertTrue(!!listRow);
                listRow.click();

                assertEquals(
                    Page.CHECKUP_DETAILS,
                    Router.getInstance().currentRoute.page);
                assertEquals(type, Router.getInstance().currentRoute.details);
              }));


  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(
              `clicking ${type} row has no effect if no issues`,
              async function() {
                passwordManager.data.checkStatus =
                    makePasswordCheckStatus({state: PasswordCheckState.IDLE});
                passwordManager.data.insecureCredentials = [];

                const section = document.createElement('checkup-section');
                document.body.appendChild(section);
                await passwordManager.whenCalled('getInsecureCredentials');
                await passwordManager.whenCalled('getPasswordCheckStatus');

                const listRow = section.shadowRoot!.querySelector<HTMLElement>(
                    `#${type}Row`);
                assertTrue(!!listRow);
                listRow.click();

                assertEquals(
                    Page.CHECKUP, Router.getInstance().currentRoute.page);
              }));
});
