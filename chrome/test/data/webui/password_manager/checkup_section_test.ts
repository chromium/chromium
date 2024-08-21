// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {PluralStringProxy} from 'chrome://password-manager/password_manager.js';
import {CheckupSubpage, Page, PasswordCheckInteraction, PasswordManagerImpl, PluralStringProxyImpl, Router, UrlParam} from 'chrome://password-manager/password_manager.js';
import {assertArrayEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup, makeInsecureCredential, makePasswordCheckStatus} from './test_util.js';

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
      'checkingPasswords',
      'compromisedPasswords',
      'compromisedPasswordsTitle',
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
    assertTrue(isVisible(section.$.checkupStatusSubLabel));
    assertEquals(
        elapsedTime, section.$.checkupStatusSubLabel.textContent!.trim());
    assertFalse(isVisible(section.$.retryButton));
    assertFalse(isVisible(section.$.spinner));
  });

  test('Running state', async function() {
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        {state: PasswordCheckState.RUNNING, totalNumber: 10, checked: 4});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    assertEquals(10, await pluralString.whenCalled('checkingPasswords'));
    await flushTasks();

    assertFalse(isVisible(section.$.checkupResult));
    assertTrue(isVisible(section.$.refreshButton));
    assertTrue(section.$.refreshButton.disabled);
    assertTrue(isVisible(section.$.checkupStatusSubLabel));
    assertEquals(
        section.i18n('checkupProgress', 4, 10),
        section.$.checkupStatusSubLabel.textContent!.trim());
    assertFalse(isVisible(section.$.retryButton));
    assertTrue(isVisible(section.$.spinner));
  });

  test('check NO_PASSWORDS state', async function() {
    passwordManager.data.checkStatus =
        makePasswordCheckStatus({state: PasswordCheckState.NO_PASSWORDS});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    assertFalse(isVisible(section.$.checkupResult));
    assertFalse(isVisible(section.$.refreshButton));
    assertTrue(isVisible(section.$.checkupStatusLabel));
    assertTrue(isVisible(section.$.checkupStatusSubLabel));
    assertEquals(
        section.i18n(
            'checkupErrorNoPasswords', section.i18n('localPasswordManager')),
        section.$.checkupStatusSubLabel.textContent!.trim());
    assertFalse(isVisible(section.$.retryButton));
    assertFalse(isVisible(section.$.spinner));
  });

  [{state: PasswordCheckState.QUOTA_LIMIT, message: 'checkupErrorQuota'},
   {state: PasswordCheckState.OFFLINE, message: 'checkupErrorOffline'},
   {state: PasswordCheckState.SIGNED_OUT, message: 'checkupErrorSignedOut'},
   {state: PasswordCheckState.OTHER_ERROR, message: 'checkupErrorGeneric'}]
      .forEach(status => test(`Error state ${status.state}`, async function() {
                 passwordManager.data.checkStatus = makePasswordCheckStatus(
                     {state: status.state, lastCheck: 'One week ago'});

                 const section = document.createElement('checkup-section');
                 document.body.appendChild(section);
                 await flushTasks();

                 assertTrue(isVisible(section.$.checkupResult));
                 assertFalse(isVisible(section.$.refreshButton));
                 assertTrue(isVisible(section.$.checkupStatusSubLabel));
                 assertEquals(
                     passwordManager.data.checkStatus.elapsedTimeSinceLastCheck,
                     section.$.checkupStatusSubLabel.textContent!.trim());
                 assertTrue(isVisible(section.$.retryButton));
                 assertFalse(isVisible(section.$.spinner));
                 assertEquals(
                     section.i18n('compromisedRowWithError'),
                     section.$.compromisedRow.label);
                 assertEquals(
                     section.i18n(
                         status.message, section.i18n('localPasswordManager')),
                     section.$.compromisedRow.subLabel);
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
    assertTrue(section.$.compromisedRow.hasAttribute('show-red-icon'));
    assertFalse(section.$.reusedRow.hasAttribute('show-yellow-icon'));
    assertTrue(section.$.weakRow.hasAttribute('show-yellow-icon'));

    // Expect a proper rear icon state
    assertFalse(section.$.compromisedRow.hasAttribute('non-clickable'));
    assertTrue(section.$.reusedRow.hasAttribute('non-clickable'));
    assertFalse(section.$.weakRow.hasAttribute('non-clickable'));
  });

  test('Number of checked sites shown', async function() {
    passwordManager.data.groups = Array(10).fill(createCredentialGroup());
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        {state: PasswordCheckState.IDLE, totalNumber: 20});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    passwordManager.whenCalled('getPasswordCheckStatus');

    await flushTasks();

    await pluralString.whenCalled('checkedPasswords');
    // getPluralString() for 'checkedPasswords' is called 2 times with 0 and 10.
    assertArrayEquals([0, 10], pluralString.getArgs('checkedPasswords'));
    assertEquals(
        'checkedPasswords', section.$.checkupStatusLabel.textContent!.trim());
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

  test('Start check automatically', async function() {
    const newParams = new URLSearchParams();
    newParams.set(UrlParam.START_CHECK, 'true');
    Router.getInstance().updateRouterParams(newParams);

    passwordManager.data.checkStatus =
        makePasswordCheckStatus({state: PasswordCheckState.IDLE});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    await flushTasks();

    await passwordManager.whenCalled('startBulkPasswordCheck');
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    assertEquals(
        PasswordCheckInteraction.START_CHECK_AUTOMATICALLY, interaction);
  });

  test('changing number of groups changes title', async function() {
    passwordManager.data.groups = Array(10).fill(createCredentialGroup());
    passwordManager.data.checkStatus = makePasswordCheckStatus(
        {state: PasswordCheckState.IDLE, totalNumber: 20});

    const section = document.createElement('checkup-section');
    document.body.appendChild(section);
    passwordManager.whenCalled('getPasswordCheckStatus');

    await flushTasks();

    await pluralString.whenCalled('checkedPasswords');
    // getPluralString() for 'checkedPasswords' is called 2 times with 0 and 10.
    assertArrayEquals([0, 10], pluralString.getArgs('checkedPasswords'));

    passwordManager.data.groups = Array(9).fill(createCredentialGroup());
    assertTrue(!!passwordManager.listeners.savedPasswordListChangedListener);
    passwordManager.listeners.savedPasswordListChangedListener([]);

    await pluralString.whenCalled('checkedPasswords');
    // getPluralString() for 'checkedPasswords' is called 3 times with 0, 10
    // and 9.
    assertArrayEquals([0, 10, 9], pluralString.getArgs('checkedPasswords'));
  });

  test('Compromised section - subheader', async function() {
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
    assertEquals(3, await pluralString.whenCalled('compromisedPasswordsTitle'));
    await flushTasks();

    // Expect string returned by PluralStringProxy.
    assertEquals('compromisedPasswords', section.$.compromisedRow.label);
    assertEquals(
        'compromisedPasswordsTitle', section.$.compromisedRow.subLabel);
  });
});
