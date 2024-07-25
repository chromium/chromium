// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {CheckupSubpage, Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createCredentialGroup, createPasswordEntry, makeInsecureCredential, makePasswordCheckStatus} from './test_util.js';

suite('PasswordManagerAppTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;
  const PasswordCheckState = chrome.passwordsPrivate.PasswordCheckState;

  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    const query = new URLSearchParams();
    Router.getInstance().updateRouterParams(query);
    return flushTasks();
  });

  test('Clicking enter during search focuses first result', async function() {
    passwordManager.data.groups = [
      createCredentialGroup({
        name: 'abc.com',
        credentials: [
          createPasswordEntry({id: 0, username: 'test1'}),
        ],
      }),
      createCredentialGroup({
        name: 'bbc.org',
        credentials: [
          createPasswordEntry({id: 1, username: 'test1'}),
        ],
      }),
    ];

    const app = document.createElement('password-manager-app');
    document.body.appendChild(app);
    app.setNarrowForTesting(false);
    await flushTasks();

    assertEquals(app.shadowRoot!.activeElement, app.$.toolbar);

    app.$.toolbar.searchField.setValue('.org');
    keyDownOn(app.$.toolbar.searchField, 0, [], 'Enter');

    const firstMatch =
        app.$.passwords.shadowRoot!.querySelector('password-list-item');
    assertTrue(!!firstMatch);
    assertEquals(app.shadowRoot!.activeElement, app.$.passwords);
    assertEquals(app.$.passwords.shadowRoot!.activeElement, firstMatch);
  });

  test(
      'Focus stays on password when navigating back from password details',
      async function() {
        passwordManager.data.groups = [
          createCredentialGroup({
            name: 'abc.com',
            credentials: [
              createPasswordEntry({id: 0, username: 'test1'}),
            ],
          }),
        ];
        passwordManager.setRequestCredentialsDetailsResponse(
            passwordManager.data.groups[0]!.entries);

        const app = document.createElement('password-manager-app');
        document.body.appendChild(app);
        app.setNarrowForTesting(false);
        await flushTasks();


        const passwordListItem =
            app.$.passwords.shadowRoot!.querySelector('password-list-item');
        assertTrue(!!passwordListItem);
        passwordListItem.click();
        await flushTasks();
        await microtasksFinished();

        // Verify that password details page is shown with back button focused.
        assertEquals(
            Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
        const passwordDetailsPage =
            app.shadowRoot!.querySelector('password-details-section');
        assertTrue(!!passwordDetailsPage);
        assertEquals(app.shadowRoot!.activeElement, passwordDetailsPage);
        assertEquals(
            passwordDetailsPage.shadowRoot!.activeElement,
            passwordDetailsPage.$.backButton);

        passwordDetailsPage.$.backButton.click();
        await flushTasks();
        await microtasksFinished();

        // Verify that passwords page is opened and the password item is
        // focused.
        assertEquals(Page.PASSWORDS, Router.getInstance().currentRoute.page);
        assertEquals(app.shadowRoot!.activeElement, app.$.passwords);
        assertEquals(
            app.$.passwords.shadowRoot!.activeElement, passwordListItem);
      });

  [CheckupSubpage.COMPROMISED, CheckupSubpage.REUSED, CheckupSubpage.WEAK]
      .forEach(
          type => test(
              `Focus stays on ${type} row when navigating from details page`,
              async function() {
                Router.getInstance().navigateTo(Page.CHECKUP);
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

                const app = document.createElement('password-manager-app');
                document.body.appendChild(app);
                app.setNarrowForTesting(false);
                await flushTasks();

                const listRow =
                    app.$.checkup.shadowRoot!.querySelector<HTMLElement>(
                        `#${type}Row`);
                assertTrue(!!listRow);
                listRow.click();
                await flushTasks();
                await microtasksFinished();

                // Verify that checkup details page is shown with back button
                // focused.
                assertEquals(
                    Page.CHECKUP_DETAILS,
                    Router.getInstance().currentRoute.page);
                const checkupDetailsPage =
                    app.shadowRoot!.querySelector('checkup-details-section');
                assertTrue(!!checkupDetailsPage);
                assertEquals(app.shadowRoot!.activeElement, checkupDetailsPage);
                assertEquals(
                    checkupDetailsPage.shadowRoot!.activeElement,
                    checkupDetailsPage.$.backButton);

                checkupDetailsPage.$.backButton.click();
                await flushTasks();
                await microtasksFinished();

                // Verify that checkup page is opened and the correct row is
                // focused.
                assertEquals(
                    Page.CHECKUP, Router.getInstance().currentRoute.page);
                assertEquals(app.shadowRoot!.activeElement, app.$.checkup);
                assertEquals(app.$.checkup.shadowRoot!.activeElement, listRow);
              }));
});
