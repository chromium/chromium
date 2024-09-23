// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CrExpandButtonElement} from 'chrome://password-manager/password_manager.js';
import {CheckupSubpage, OpenWindowProxyImpl, Page, PasswordCheckInteraction, PasswordManagerImpl, PluralStringProxyImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createCredentialGroup, makeInsecureCredential, makePasswordManagerPrefs} from './test_util.js';

suite('CheckupDetailsSectionTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let openWindowProxy: TestOpenWindowProxy;
  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    pluralString = new TestPluralStringProxy();
    PluralStringProxyImpl.setInstance(pluralString);
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

            if (type === CheckupSubpage.COMPROMISED) {
              // getPluralString() should be called 2 times: 1 for page title,
              // and 1 more for page subtitle.
              assertEquals(2, pluralString.getCallCount('getPluralString'));
            } else {
              assertEquals(
                  loadTimeData.getString(`${type}PasswordsTitle`),
                  section.$.subtitle.textContent!.trim());
            }
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
    await dismissedButton.updateComplete;

    assertTrue(isVisible(listItemElements[0]));
  });

  test('Reused issues shown correctly', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.REUSED);
    const insecurePasswords = [
      makeInsecureCredential({url: 'Some app', username: 'viking', id: 0}),
      makeInsecureCredential({url: 'example.com', username: 'user', id: 1}),
      makeInsecureCredential({url: 'test.com', username: 'Lalala', id: 2}),
      makeInsecureCredential(
          {url: 'accounts.google.com', username: 'corporateEmail', id: 3}),
      makeInsecureCredential(
          {url: 'super.secure.com', username: 'admin', id: 4}),
    ];
    passwordManager.data.groups = insecurePasswords.map(
        entry => createCredentialGroup(
            {name: entry.affiliatedDomains[0]!.name, credentials: [entry]}));
    passwordManager.data.credentialWithReusedPassword = [
      {entries: insecurePasswords.slice(0, 3).sort(() => Math.random() - 0.5)},
      {entries: insecurePasswords.slice(3, 5).sort(() => Math.random() - 0.5)},
    ];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await passwordManager.whenCalled('getCredentialsWithReusedPassword');
    await pluralString.whenCalled('getPluralString');
    // getPluralString() should be called 3 times: 1 for title, and 2 more for
    // each reused password.
    assertEquals(3, pluralString.getCallCount('getPluralString'));
    await flushTasks();

    const listItemElements =
        section.shadowRoot!.querySelectorAll('checkup-list-item');
    assertEquals(insecurePasswords.length, listItemElements.length);

    for (let index = 0; index < listItemElements.length; ++index) {
      const expectedCredential = insecurePasswords[index]!;
      const listItemElement = listItemElements[index];

      assertTrue(!!listItemElement);
      assertEquals(
          expectedCredential.affiliatedDomains[0]!.name,
          listItemElement.$.shownUrl.textContent!.trim());
      assertEquals(
          expectedCredential.username,
          listItemElement.$.username.textContent!.trim());
      const leakType = listItemElement.shadowRoot!.querySelector('#leakType');
      assertFalse(!!leakType);

      const elapsedTime =
          listItemElement.shadowRoot!.querySelector('#elapsedTime');
      assertFalse(!!elapsedTime);
    }
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
    section.prefs = makePasswordManagerPrefs();
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

    const section = document.createElement('checkup-details-section');
    section.prefs = makePasswordManagerPrefs();
    section.prefs.profile.password_dismiss_compromised_alert.value = false;
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

  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(`Change password click for ${type}`, async function() {
            Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, type);

            const insecureCredential = makeInsecureCredential({
              id: Math.floor(Math.random() * 1000),
              url: 'test.com',
              username: 'viking',
              types: [
                CompromiseType.LEAKED,
                CompromiseType.WEAK,
                CompromiseType.REUSED,
              ],
            });
            passwordManager.data.insecureCredentials = [insecureCredential];
            passwordManager.data.credentialWithReusedPassword =
                [{entries: [insecureCredential]}];

            const section = document.createElement('checkup-details-section');
            document.body.appendChild(section);
            await passwordManager.whenCalled('getInsecureCredentials');
            if (type === CheckupSubpage.REUSED) {
              await passwordManager.whenCalled(
                  'getCredentialsWithReusedPassword');
            }
            await pluralString.whenCalled('getPluralString');
            await flushTasks();

            const listItemElements =
                section.shadowRoot!.querySelectorAll('checkup-list-item');
            assertEquals(1, listItemElements.length);
            assertTrue(!!listItemElements[0]);
            assertTrue(isVisible(listItemElements[0]));


            // Verify that 'Already change password?' link is hidden.
            const alreadyChange =
                listItemElements[0].shadowRoot!.querySelector<HTMLElement>(
                    '#alreadyChanged');
            assertTrue(!!alreadyChange);
            assertTrue(alreadyChange.hidden);

            const changePassword =
                listItemElements[0].shadowRoot!.querySelector<HTMLElement>(
                    '#changePasswordButton');
            assertTrue(!!changePassword);

            changePassword.click();
            const url = await openWindowProxy.whenCalled('openUrl');
            assertEquals(url, insecureCredential.changePasswordUrl);
            await flushTasks();

            // Verify that 'Already change password?' link is visible.
            assertFalse(alreadyChange.hidden);
          }));

  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(`Change password in app for ${type}`, async function() {
            Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, type);

            const insecureCredential = makeInsecureCredential({
              url: 'test.com',
              username: 'viking',
              types: [
                CompromiseType.LEAKED,
                CompromiseType.WEAK,
                CompromiseType.REUSED,
              ],
            });
            insecureCredential.changePasswordUrl = undefined;
            passwordManager.data.insecureCredentials = [insecureCredential];
            passwordManager.data.credentialWithReusedPassword =
                [{entries: [insecureCredential]}];

            const section = document.createElement('checkup-details-section');
            document.body.appendChild(section);
            await passwordManager.whenCalled('getInsecureCredentials');
            if (type === CheckupSubpage.REUSED) {
              await passwordManager.whenCalled(
                  'getCredentialsWithReusedPassword');
            }
            await pluralString.whenCalled('getPluralString');
            await flushTasks();

            const listItemElements =
                section.shadowRoot!.querySelectorAll('checkup-list-item');
            assertEquals(1, listItemElements.length);
            assertTrue(!!listItemElements[0]);
            assertTrue(isVisible(listItemElements[0]));

            // Verify that 'Change password' button is hidden.
            const changePassword =
                listItemElements[0].shadowRoot!.querySelector(
                    '#changePasswordButton');
            assertFalse(!!changePassword);

            const changeInAppString =
                listItemElements[0].shadowRoot!.querySelector(
                    '#changePasswordInApp');
            assertTrue(!!changeInAppString);
            assertTrue(isVisible(changeInAppString));
            assertEquals(
                loadTimeData.getString('changePasswordInApp'),
                changeInAppString.textContent?.trim());
          }));

  test('Edit action button works', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    const credential = makeInsecureCredential({
      id: 0,
      url: 'test.com',
      username: 'viking',
      password: 'pass',
      types: [
        CompromiseType.LEAKED,
      ],
    });
    credential.affiliatedDomains = [createAffiliatedDomain('test.com')];
    passwordManager.data.insecureCredentials = [credential];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);

    // Click more actions button.
    listItem.$.more.click();
    // Set up response for requestCredentialsDetails() to simulate successful
    // reauth.
    passwordManager.setRequestCredentialsDetailsResponse([credential]);

    section.$.menuEditPassword.click();
    await passwordManager.whenCalled('requestCredentialsDetails');
    await flushTasks();

    const editDialog =
        listItem.shadowRoot!.querySelector('edit-password-dialog');
    assertTrue(!!editDialog);
    assertTrue(editDialog.$.dialog.open);
  });

  test('No edit if auth failed', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    const credential = makeInsecureCredential({
      id: 0,
      url: 'test.com',
      username: 'viking',
      password: 'pass',
      types: [
        CompromiseType.LEAKED,
      ],
    });
    credential.affiliatedDomains = [createAffiliatedDomain('test.com')];
    passwordManager.data.insecureCredentials = [credential];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);

    // Click more actions button.
    listItem.$.more.click();

    section.$.menuEditPassword.click();
    await passwordManager.whenCalled('requestCredentialsDetails');
    await flushTasks();

    const editDialog =
        listItem.shadowRoot!.querySelector('edit-password-dialog');
    assertFalse(!!editDialog);
  });

  test('Edit dialog shown during already change flow', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    const credential = makeInsecureCredential({
      id: 0,
      url: 'test.com',
      username: 'viking',
      password: 'pass',
      types: [
        CompromiseType.LEAKED,
      ],
    });
    credential.affiliatedDomains = [createAffiliatedDomain('test.com')];
    passwordManager.data.insecureCredentials = [credential];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);

    // Click 'Change password'
    const changePassword = listItem.shadowRoot!.querySelector<HTMLElement>(
        '#changePasswordButton');
    assertTrue(!!changePassword);
    changePassword.click();

    // Click 'Already change password?'
    const alreadyChange =
        listItem.shadowRoot!.querySelector<HTMLElement>('#alreadyChanged');
    assertTrue(!!alreadyChange);
    alreadyChange.click();
    await flushTasks();

    // Verify edit disclaimer dialog is shown.
    const editDisclaimer =
        listItem.shadowRoot!.querySelector('edit-password-disclaimer-dialog');
    assertTrue(!!editDisclaimer);
    assertTrue(editDisclaimer.$.dialog.open);

    // Set up response for requestCredentialsDetails() to simulate successful
    // reauth and click 'Edit'.
    passwordManager.setRequestCredentialsDetailsResponse([credential]);
    editDisclaimer.$.edit.click();

    await passwordManager.whenCalled('requestCredentialsDetails');
    await flushTasks();

    const editDialog =
        listItem.shadowRoot!.querySelector('edit-password-dialog');
    assertTrue(!!editDialog);
    assertTrue(editDialog.$.dialog.open);
  });

  test('Delete insecure password', async function() {
    Router.getInstance().navigateTo(
        Page.CHECKUP_DETAILS, CheckupSubpage.COMPROMISED);
    const credential = makeInsecureCredential({
      id: 0,
      url: 'test.com',
      username: 'viking',
      types: [
        CompromiseType.LEAKED,
      ],
    });
    credential.affiliatedDomains = [createAffiliatedDomain('test.com')];
    passwordManager.data.insecureCredentials = [credential];

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    await flushTasks();

    const listItem = section.shadowRoot!.querySelector('checkup-list-item');
    assertTrue(!!listItem);

    // Click more actions button.
    listItem.$.more.click();

    section.$.menuDeletePassword.click();
    await flushTasks();

    const deleteDialog =
        listItem.shadowRoot!.querySelector('delete-password-disclaimer-dialog');
    assertTrue(!!deleteDialog);
    assertTrue(deleteDialog.$.dialog.open);

    // Change password URL gets linkified.
    assertTrue(isVisible(deleteDialog.$.link));
    assertFalse(isVisible(deleteDialog.$.text));

    // Click 'Delete password'.
    deleteDialog.$.delete.click();
    const interaction =
        await passwordManager.whenCalled('recordPasswordCheckInteraction');
    const params = await passwordManager.whenCalled('removeCredential');
    assertEquals(params.id, credential.id);
    assertEquals(params.fromStores, credential.storedIn);
    assertEquals(PasswordCheckInteraction.REMOVE_PASSWORD, interaction);
  });
});
