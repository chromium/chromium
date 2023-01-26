// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CheckupSubpage, CrExpandButtonElement, Page, PasswordManagerImpl, PrefsBrowserProxyImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {createCredentialGroup, makeInsecureCredential, makePasswordManagerPrefs} from './test_util.js';

suite('CheckupDetailsSectionTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;
  let prefsProxy: TestPrefsBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
    prefsProxy = new TestPrefsBrowserProxy();
    prefsProxy.prefs = makePasswordManagerPrefs();
    PrefsBrowserProxyImpl.setInstance(prefsProxy);
    Router.getInstance().navigateTo(Page.CHECKUP);
    return flushTasks();
  });

  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(`Title shown correctly for ${type}`, async function() {
            Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, type);
            passwordManager.data.insecureCredentials = [
              makeInsecureCredential({
                types: [
                  CompromiseType.LEAKED,
                  CompromiseType.REUSED,
                  CompromiseType.WEAK,
                ],
              }),
              makeInsecureCredential({
                types: [
                  CompromiseType.PHISHED,
                  CompromiseType.REUSED,
                  CompromiseType.WEAK,
                ],
              }),
            ];

            const section = document.createElement('checkup-details-section');
            document.body.appendChild(section);
            await passwordManager.whenCalled('getInsecureCredentials');
            const params = await pluralString.whenCalled('getPluralString');
            await flushTasks();

            assertEquals(type + 'Passwords', params.messageName);
            assertEquals(2, params.itemCount);

            assertEquals(
                loadTimeData.getString(`${type}PasswordsTitle`),
                section.$.subtitle.textContent!.trim());
            assertEquals(
                loadTimeData.getString(`${type}PasswordsDescription`),
                section.$.description.textContent!.trim());
          }));

  test('Compromised issues shown correctly', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        url: 'test.com',
        username: 'viking',
        types: [
          CompromiseType.LEAKED,
        ],
        elapsedMinSinceCompromise: 1,
      }),
      makeInsecureCredential({
        url: 'example.com',
        username: 'justUser',
        types: [
          CompromiseType.PHISHED,
        ],
        elapsedMinSinceCompromise: 10,
      }),
      makeInsecureCredential({
        url: 'www.bestSite.com',
        username: 'random',
        types: [
          CompromiseType.LEAKED,
          CompromiseType.PHISHED,
        ],
        elapsedMinSinceCompromise: 100,
      }),
    ];
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'Affiliation.com',
        credentials: passwordManager.data.insecureCredentials,
      }),
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await passwordManager.whenCalled('getCredentialGroups');
    const params = await pluralString.whenCalled('getPluralString');
    await flushTasks();

    assertEquals('compromisedPasswords', params.messageName);
    assertEquals(3, params.itemCount);

    const listItemElements =
        section.shadowRoot!.querySelectorAll('checkup-list-item');
    assertEquals(
        listItemElements.length,
        passwordManager.data.insecureCredentials.length);

    const expectedType = [
      loadTimeData.getString('leakedPassword'),
      loadTimeData.getString('phishedPassword'),
      loadTimeData.getString('phishedAndLeakedPassword'),
    ];

    for (let index = 0; index < listItemElements.length; ++index) {
      const expectedCredential =
          passwordManager.data.insecureCredentials[index]!;
      const listItemElement = listItemElements[index];

      assertTrue(!!listItemElement);
      assertEquals(
          passwordManager.data.groups[0]?.name,
          listItemElement.$.shownUrl.textContent!.trim());
      assertEquals(
          expectedCredential.username,
          listItemElement.$.username.textContent!.trim());
      const compromiseType =
          listItemElement.shadowRoot!.querySelector('#compromiseType');

      assertTrue(!!compromiseType);
      assertTrue(isVisible(compromiseType));
      assertEquals(expectedType[index]!, compromiseType.textContent!.trim());

      const elapsedTime =
          listItemElement.shadowRoot!.querySelector('#elapsedTime');
      assertTrue(!!elapsedTime);
      assertTrue(isVisible(elapsedTime));
      assertEquals(
          expectedCredential.compromisedInfo?.elapsedTimeSinceCompromise,
          elapsedTime.textContent!.trim());
    }
  });

  test('Weak issues shown correctly', async function() {
    Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, CheckupSubpage.WEAK);
    passwordManager.data.insecureCredentials = [makeInsecureCredential({
      url: 'test.com',
      username: 'viking',
      types: [
        CompromiseType.WEAK,
      ],
      elapsedMinSinceCompromise: 1,
    })];
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'Best test site',
        credentials: passwordManager.data.insecureCredentials,
      }),
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await passwordManager.whenCalled('getCredentialGroups');
    const params = await pluralString.whenCalled('getPluralString');
    await flushTasks();

    assertEquals('weakPasswords', params.messageName);
    assertEquals(1, params.itemCount);

    const listItemElements =
        section.shadowRoot!.querySelectorAll('checkup-list-item');
    assertEquals(1, listItemElements.length);
    const weakItem = listItemElements[0];

    assertTrue(!!weakItem);
    assertEquals(
        passwordManager.data.groups[0]!.name,
        weakItem.$.shownUrl.textContent!.trim());
    assertEquals('viking', weakItem.$.username.textContent!.trim());

    assertFalse(!!weakItem.shadowRoot!.querySelector('#compromiseType'));
    assertFalse(!!weakItem.shadowRoot!.querySelector('#elapsedTime'));
  });

  test('Muted compromised issues shown correctly', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        url: 'test.com',
        username: 'viking',
        types: [
          CompromiseType.LEAKED,
        ],
        elapsedMinSinceCompromise: 1,
        isMuted: false,
      }),
      makeInsecureCredential({
        url: 'example.com',
        username: 'admin',
        types: [
          CompromiseType.PHISHED,
        ],
        elapsedMinSinceCompromise: 1,
        isMuted: false,
      }),
      makeInsecureCredential({
        url: 'example.com',
        username: 'justUser',
        types: [
          CompromiseType.PHISHED,
        ],
        elapsedMinSinceCompromise: 10,
        isMuted: true,
      }),
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    const params = await pluralString.whenCalled('getPluralString');
    await flushTasks();

    assertEquals('compromisedPasswords', params.messageName);
    assertEquals(2, params.itemCount);

    const dismissedButton =
        section.shadowRoot!.querySelector<CrExpandButtonElement>(
            '#expandMutedCompromisedCredentialsButton');
    assertTrue(!!dismissedButton);
    assertTrue(isVisible(dismissedButton));

    const mutedCredentialsList =
        section.shadowRoot!.querySelector('#mutedCredentialsList');
    assertTrue(!!mutedCredentialsList);

    const listItemElements =
        mutedCredentialsList.querySelectorAll<HTMLElement>('checkup-list-item');
    assertEquals(1, listItemElements.length);
    assertTrue(!!listItemElements[0]);
    assertFalse(isVisible(listItemElements[0]));

    dismissedButton.click();

    assertTrue(isVisible(listItemElements[0]));
  });

  test('Show/Hide action button works', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        id: 0,
        url: 'test.com',
        username: 'viking',
        types: [
          CompromiseType.LEAKED,
        ],
      }),
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);
    assertTrue(isVisible(listItem));
    assertEquals('password', listItem.$.insecurePassword.type);

    assertFalse(isVisible(section.$.moreActionsMenu));

    // Click more actions button
    listItem.$.more.click();

    assertTrue(isVisible(section.$.menuShowPassword));
    assertEquals(
        loadTimeData.getString('showPassword'),
        section.$.menuShowPassword.textContent?.trim());

    const credentialWithPassword = passwordManager.data.insecureCredentials[0]!;
    credentialWithPassword.password = 'pAssWoRd';
    passwordManager.setRequestCredentialsDetailsResponse(
        [credentialWithPassword]);

    section.$.menuShowPassword.click();
    await passwordManager.whenCalled('requestCredentialsDetails');

    assertEquals('text', listItem.$.insecurePassword.type);
    assertEquals(
        credentialWithPassword.password, listItem.$.insecurePassword.value);

    listItem.$.more.click();
    assertTrue(isVisible(section.$.menuShowPassword));
    assertEquals(
        loadTimeData.getString('hidePassword'),
        section.$.menuShowPassword.textContent?.trim());
    section.$.menuShowPassword.click();

    assertEquals('password', listItem.$.insecurePassword.type);
  });

  test('Mute button works', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        id: 0,
        url: 'test.com',
        username: 'viking',
        types: [
          CompromiseType.LEAKED,
        ],
      }),
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);
    assertTrue(isVisible(listItem));

    // Click more actions button
    listItem.$.more.click();

    const muteButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuMuteUnmuteButton');
    assertTrue(!!muteButton);
    assertTrue(isVisible(muteButton));
    assertEquals(
        loadTimeData.getString('muteCompromisedPassword'),
        muteButton.textContent?.trim());

    muteButton.click();
    await passwordManager.whenCalled('muteInsecureCredential');
  });

  test('Unmute button works', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        id: 0,
        url: 'test.com',
        username: 'viking',
        types: [
          CompromiseType.LEAKED,
        ],
        isMuted: true,
      }),
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    // Expand dismissed compromised credentials.
    const expandButton = section.shadowRoot!.querySelector<HTMLElement>(
        '#expandMutedCompromisedCredentialsButton');
    assertTrue(!!expandButton);
    assertTrue(isVisible(expandButton));
    expandButton.click();
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);
    assertFalse(isVisible(section.$.moreActionsMenu));

    // Click more actions button
    listItem.$.more.click();

    const unMuteButton =
        section.shadowRoot!.querySelector<HTMLElement>('#menuMuteUnmuteButton');
    assertTrue(!!unMuteButton);
    assertTrue(isVisible(unMuteButton));
    assertEquals(
        loadTimeData.getString('unmuteCompromisedPassword'),
        unMuteButton.textContent?.trim());

    unMuteButton.click();
    await passwordManager.whenCalled('unmuteInsecureCredential');
  });

  test('Mute button disabled by pref', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        id: 0,
        url: 'test.com',
        username: 'viking',
        types: [
          CompromiseType.LEAKED,
        ],
      }),
    ];

    prefsProxy.prefs = [
      {
        key: 'profile.password_dismiss_compromised_alert',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);
    assertTrue(isVisible(listItem));

    // Click more actions button
    listItem.$.more.click();

    const muteButton = section.shadowRoot!.querySelector<HTMLButtonElement>(
        '#menuMuteUnmuteButton');

    assertTrue(!!muteButton);
    assertTrue(isVisible(muteButton));
    assertTrue(muteButton.disabled);
  });
});
