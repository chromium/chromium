// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CheckupSubpage, Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestPluralStringProxy} from 'chrome://webui-test/test_plural_string_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeInsecureCredential} from './test_util.js';

suite('CheckupDetailsSectionTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let passwordManager: TestPasswordManagerProxy;
  let pluralString: TestPluralStringProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
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

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
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
          expectedCredential.urls.shown,
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

    const section = document.createElement('checkup-details-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getInsecureCredentials');
    const params = await pluralString.whenCalled('getPluralString');
    await flushTasks();

    assertEquals('weakPasswords', params.messageName);
    assertEquals(1, params.itemCount);

    const listItemElements =
        section.shadowRoot!.querySelectorAll('checkup-list-item');
    assertEquals(1, listItemElements.length);
    const weakItem = listItemElements[0];

    assertTrue(!!weakItem);
    assertEquals('test.com', weakItem.$.shownUrl.textContent!.trim());
    assertEquals('viking', weakItem.$.username.textContent!.trim());

    assertFalse(!!weakItem.shadowRoot!.querySelector('#compromiseType'));
    assertFalse(!!weakItem.shadowRoot!.querySelector('#elapsedTime'));
  });
});
