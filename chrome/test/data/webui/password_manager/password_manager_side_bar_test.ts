// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {PasswordManagerSideBarElement} from 'chrome://password-manager/password_manager.js';
import {CheckupSubpage, Page, PasswordManagerImpl, Router} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeInsecureCredential} from './test_util.js';

suite('PasswordManagerSideBarTest', function() {
  const CompromiseType = chrome.passwordsPrivate.CompromiseType;

  let sidebar: PasswordManagerSideBarElement;
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    sidebar = document.createElement('password-manager-side-bar');
    document.body.appendChild(sidebar);
    return flushTasks();
  });

  test('check layout', function() {
    assertTrue(isVisible(sidebar));
    const sideBarEntries = sidebar.shadowRoot!.querySelectorAll('a');
    assertEquals(3, sideBarEntries.length);
  });

  [Page.PASSWORDS, Page.CHECKUP, Page.SETTINGS].forEach(
      page => test(`clicking ${page} updates path`, function() {
        const differentPage =
            page === Page.PASSWORDS ? Page.CHECKUP : Page.PASSWORDS;
        Router.getInstance().navigateTo(differentPage);
        assertEquals(differentPage, Router.getInstance().currentRoute.page);

        const element =
            sidebar.shadowRoot!.querySelector<HTMLElement>(`#${page}`)!;
        element.click();
        assertEquals(page, Router.getInstance().currentRoute.page);
      }));

  [Page.PASSWORDS, Page.CHECKUP, Page.SETTINGS].forEach(
      page => test(`navigating to ${page} updates selected item`, async () => {
        const whenSelected = eventToPromise('iron-select', sidebar.$.menu);
        Router.getInstance().navigateTo(page);
        await whenSelected;
        assertEquals(page, Router.getInstance().currentRoute.page);
        const selectedItem =
            sidebar.$.menu.querySelector<HTMLElement>('.selected');
        assertTrue(!!selectedItem);
        assertEquals(page, selectedItem.id);
      }));

  test('navigating to password details selects passwords tab', async () => {
    const whenSelected = eventToPromise('iron-select', sidebar.$.menu);
    Router.getInstance().navigateTo(Page.PASSWORD_DETAILS, 'google.com');
    await whenSelected;
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    const selectedItem = sidebar.$.menu.querySelector<HTMLElement>('.selected');
    assertTrue(!!selectedItem);
    assertEquals(Page.PASSWORDS, selectedItem.id);
  });

  test('navigating to checkup details selects checkup tab', async () => {
    const whenSelected = eventToPromise('iron-select', sidebar.$.menu);
    Router.getInstance().navigateTo(Page.CHECKUP_DETAILS, CheckupSubpage.WEAK);
    await whenSelected;
    assertEquals(Page.CHECKUP_DETAILS, Router.getInstance().currentRoute.page);
    const selectedItem = sidebar.$.menu.querySelector<HTMLElement>('.selected');
    assertTrue(!!selectedItem);
    assertEquals(Page.CHECKUP, selectedItem.id);
  });

  test(
      'no compromised element when no compromised passwords exist',
      async function() {
        passwordManager.data.insecureCredentials = [];
        assertTrue(!!passwordManager.listeners.insecureCredentialsListener);
        // The test password manager proxy does not invoke the listener when the
        // data changes, so mimic that action to ensure the element responds.
        passwordManager.listeners.insecureCredentialsListener(
            passwordManager.data.insecureCredentials);
        await flushTasks();

        assertFalse(isVisible(sidebar.$.compromisedPasswords));
      });

  test('reused or weak compromised password count displayed', async function() {
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        types: [
          CompromiseType.REUSED,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.WEAK,
        ],
      }),
    ];
    assertTrue(!!passwordManager.listeners.insecureCredentialsListener);
    passwordManager.listeners.insecureCredentialsListener(
        passwordManager.data.insecureCredentials);
    await flushTasks();
    // The compromised passwords are not of the correct type; do not display the
    // count.
    assertFalse(isVisible(sidebar.$.compromisedPasswords));
  });

  test('muted compromised password count not displayed', async function() {
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        types: [
          CompromiseType.LEAKED,
        ],
        isMuted: true,
      }),
    ];
    assertTrue(!!passwordManager.listeners.insecureCredentialsListener);
    passwordManager.listeners.insecureCredentialsListener(
        passwordManager.data.insecureCredentials);
    await flushTasks();
    // The compromised is of the correct type, but muted; do not display the
    // count.
    assertFalse(isVisible(sidebar.$.compromisedPasswords));
  });

  test('compromised password count displayed', async function() {
    passwordManager.data.insecureCredentials = [
      makeInsecureCredential({
        types: [
          CompromiseType.LEAKED,
          CompromiseType.PHISHED,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.LEAKED,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.PHISHED,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.REUSED,
        ],
      }),
      makeInsecureCredential({
        types: [
          CompromiseType.WEAK,
        ],
      }),
    ];
    assertTrue(!!passwordManager.listeners.insecureCredentialsListener);
    passwordManager.listeners.insecureCredentialsListener(
        passwordManager.data.insecureCredentials);
    await flushTasks();
    // The three passwords with types of one of LEAKED or PHISHED should be
    // reflected in the sidebar. The others should not.
    assertTrue(isVisible(sidebar.$.compromisedPasswords));
    assertEquals('3', sidebar.$.compromisedPasswords.innerText);
  });

  test('more than 100 compromised passwords shows 99+', async function() {
    passwordManager.data.insecureCredentials = [];
    for (let i = 0; i < 100; i++) {
      passwordManager.data.insecureCredentials.push(
          makeInsecureCredential({
            types: [
              CompromiseType.LEAKED,
            ],
          }),
      );
    }

    assertTrue(!!passwordManager.listeners.insecureCredentialsListener);
    passwordManager.listeners.insecureCredentialsListener(
        passwordManager.data.insecureCredentials);
    await flushTasks();
    // There are three digits of compromised passwords, which should
    // overflow and show '99+'.
    assertTrue(isVisible(sidebar.$.compromisedPasswords));
    assertEquals('99+', sidebar.$.compromisedPasswords.innerText);
  });

  test('exactly 99 compromised passwords', async function() {
    passwordManager.data.insecureCredentials = [];
    for (let i = 0; i < 99; i++) {
      passwordManager.data.insecureCredentials.push(
          makeInsecureCredential({
            types: [
              CompromiseType.LEAKED,
            ],
          }),
      );
    }

    assertTrue(!!passwordManager.listeners.insecureCredentialsListener);
    passwordManager.listeners.insecureCredentialsListener(
        passwordManager.data.insecureCredentials);
    await flushTasks();
    // Verify that the overflow starts at 100, and is not off-by-one.
    assertTrue(isVisible(sidebar.$.compromisedPasswords));
    assertEquals('99', sidebar.$.compromisedPasswords.innerText);
  });
});
