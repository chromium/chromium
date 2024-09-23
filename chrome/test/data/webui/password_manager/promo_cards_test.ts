// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {PasswordsSectionElement} from 'chrome://password-manager/password_manager.js';
import {Page, PasswordManagerImpl, PromoCardsProxyImpl, Router, SyncBrowserProxyImpl, UrlParam} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestPromoCardsProxy} from './test_promo_cards_browser_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createCredentialGroup, createPasswordEntry} from './test_util.js';

suite('PasswordsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let promoCardsProxy: TestPromoCardsProxy;
  let syncProxy: TestSyncBrowserProxy;

  async function createPasswordsSection(): Promise<PasswordsSectionElement> {
    const section: PasswordsSectionElement =
        document.createElement('passwords-section');
    document.body.appendChild(section);
    await passwordManager.whenCalled('getCredentialGroups');
    await flushTasks();

    return section;
  }

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    promoCardsProxy = new TestPromoCardsProxy();
    PromoCardsProxyImpl.setInstance(promoCardsProxy);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    Router.getInstance().updateRouterParams(new URLSearchParams());
    return flushTasks();
  });

  test('promo card shown', async function() {
    promoCardsProxy.promo = {
      id: 'test_promo',
      title: 'Hello there',
      description: 'This is a promo card.',
    };

    const section = await createPasswordsSection();
    let promoCardElement = section.shadowRoot!.querySelector('promo-card');

    // Verify promo card is shown.
    assertTrue(!!promoCardElement);
    assertEquals(
        promoCardsProxy.promo!.title,
        promoCardElement.$.title.textContent!.trim());
    assertEquals(
        promoCardsProxy.promo!.description,
        promoCardElement.$.description.textContent!.trim());
    assertFalse(isVisible(promoCardElement.$.actionButton));
    const shownImage = promoCardElement.shadowRoot!.querySelector('img');
    assertTrue(!!shownImage);
    assertEquals(
        'chrome://password-manager/images/test_promo.svg', shownImage.src);

    // Click close button.
    promoCardElement.$.closeButton.click();
    assertEquals(
        promoCardsProxy.promo?.id,
        await promoCardsProxy.whenCalled('recordPromoDismissed'));
    await flushTasks();

    // Verify that the promo card is hidden.
    promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(!!promoCardElement);
  });

  test('password checkup promo', async function() {
    promoCardsProxy.promo = {
      id: 'password_checkup_promo',
      title: 'Checkup promo',
      description: 'Checkup promo description.',
      actionButtonText: 'Start check',
    };

    const section = await createPasswordsSection();
    let promoCardElement = section.shadowRoot!.querySelector('promo-card');

    // Verify promo card is shown.
    assertTrue(!!promoCardElement);
    assertTrue(isVisible(promoCardElement.$.actionButton));

    // Click action button button and verify we navigated to checkup page and
    // started password checkup.
    promoCardElement.$.actionButton.click();
    assertEquals(Page.CHECKUP, Router.getInstance().currentRoute.page);
    assertEquals(
        'true',
        String(Router.getInstance().currentRoute.queryParameters.get(
            UrlParam.START_CHECK)));
    await flushTasks();

    // Verify that the promo card is hidden.
    promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(!!promoCardElement);
  });

  test('shortcut promo', async function() {
    promoCardsProxy.promo = {
      id: 'password_shortcut_promo',
      title: 'Shortcut promo',
      description: 'Shortcut promo description.',
      actionButtonText: 'Add shortcut',
    };

    const section = await createPasswordsSection();
    let promoCardElement = section.shadowRoot!.querySelector('promo-card');

    // Verify promo card is shown.
    assertTrue(!!promoCardElement);
    assertTrue(isVisible(promoCardElement.$.actionButton));

    // Click action button button and verify we navigated to checkup page and
    // started password checkup.
    promoCardElement.$.actionButton.click();
    await passwordManager.whenCalled('showAddShortcutDialog');
    await flushTasks();

    // Verify that the promo card is hidden.
    promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(!!promoCardElement);
  });

  test('move passwords promo hidden if no local passwords', async function() {
    promoCardsProxy.promo = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageEnabled = true;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry({
        username: 'user',
        id: 0,
        inProfileStore: false,
        inAccountStore: true,
      })],
    })];
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();
    const promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(!!promoCardElement);
  });

  test('move passwords promo hidden if butter disabled', async function() {
    promoCardsProxy.promo = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageEnabled = false;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry(
          {username: 'user', id: 0, inProfileStore: true})],
    })];
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();
    const promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(!!promoCardElement);
  });

  test('move passwords promo hidden if sync issues', async function() {
    promoCardsProxy.promo = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageEnabled = true;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry(
          {username: 'user', id: 0, inProfileStore: true})],
    })];
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: false,
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();
    const promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(!!promoCardElement);
  });

  test('move passwords promo visible is requirements met', async function() {
    promoCardsProxy.promo = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageEnabled = true;

    const password = createPasswordEntry({
      id: 1234,
      username: 'user1',
      password: 'sTr0nGp@@s',
      affiliatedDomains: [createAffiliatedDomain('test.com')],
      inProfileStore: true,
    });

    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [password],
    })];
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    passwordManager.setRequestCredentialsDetailsResponse([password]);

    const section = await createPasswordsSection();
    const promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertTrue(!!promoCardElement);
    assertTrue(isVisible(promoCardElement.$.actionButton));

    promoCardElement.$.actionButton.click();
    await flushTasks();

    const moveDialog =
        section.shadowRoot!.querySelector('move-passwords-dialog');
    assertTrue(!!moveDialog);
    assertTrue(isVisible(moveDialog.$.move));
  });

  test('screenlock promo', async function() {
    promoCardsProxy.promo = {
      id: 'screenlock_reauth_promo',
      title: 'Screenlock reauth promo',
      description: 'Screenlock reauth promo description.',
      actionButtonText: 'Enable screenlock reauth',
    };
    passwordManager.setSwitchBiometricAuthBeforeFillingStateResponse(true);

    const section = await createPasswordsSection();
    let promoCardElement = section.shadowRoot!.querySelector('promo-card');

    // Verify promo card is shown.
    assertTrue(!!promoCardElement);
    assertTrue(isVisible(promoCardElement.$.actionButton));

    // Click action button button and verify that authentication started.
    promoCardElement.$.actionButton.click();
    await passwordManager.whenCalled('switchBiometricAuthBeforeFillingState');
    await flushTasks();

    // Verify that the promo card is hidden.
    promoCardElement = section.shadowRoot!.querySelector('promo-card');
    assertFalse(isVisible(promoCardElement));
  });
});
