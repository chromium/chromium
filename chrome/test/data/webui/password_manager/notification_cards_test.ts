// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {PasswordsSectionElement} from 'chrome://password-manager/password_manager.js';
import {BatchUploadPasswordsEntryPoint, NotificationCardsProxyImpl, Page, PasswordManagerImpl, Router, SyncBrowserProxyImpl, UrlParam} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestNotificationCardsProxy} from './test_notification_cards_browser_proxy.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createCredentialGroup, createPasswordEntry} from './test_util.js';

suite('PasswordsSectionTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let notificationCardsProxy: TestNotificationCardsProxy;
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
    notificationCardsProxy = new TestNotificationCardsProxy();
    NotificationCardsProxyImpl.setInstance(notificationCardsProxy);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    Router.getInstance().updateRouterParams(new URLSearchParams());
    return flushTasks();
  });

  test('notification card shown', async function() {
    notificationCardsProxy.card = {
      id: 'test_promo',
      title: 'Hello there',
      description: 'This is a notification card.',
    };

    const section = await createPasswordsSection();
    let cardElement = section.shadowRoot!.querySelector('notification-card');

    // Verify notification card is shown.
    assertTrue(!!cardElement);
    assertEquals(
        notificationCardsProxy.card.title,
        cardElement.$.title.textContent.trim());
    assertEquals(
        notificationCardsProxy.card.description,
        cardElement.$.description.textContent.trim());
    assertFalse(isVisible(cardElement.$.actionButton));
    const shownImage = cardElement.shadowRoot!.querySelector('img');
    assertTrue(!!shownImage);
    assertEquals(
        'chrome://password-manager/images/test_promo.svg', shownImage.src);

    // Click close button.
    cardElement.$.closeButton.click();
    assertEquals(
        notificationCardsProxy.card?.id,
        await notificationCardsProxy.whenCalled('recordNotificationDismissed'));
    await flushTasks();

    // Verify that the notification card is hidden.
    cardElement = section.shadowRoot!.querySelector('notification-card');
    assertFalse(!!cardElement);
  });

  test('password checkup card', async function() {
    notificationCardsProxy.card = {
      id: 'password_checkup_promo',
      title: 'Checkup promo',
      description: 'Checkup promo description.',
      actionButtonText: 'Start check',
    };

    const section = await createPasswordsSection();
    let cardElement = section.shadowRoot!.querySelector('notification-card');

    // Verify notification card is shown.
    assertTrue(!!cardElement);
    assertTrue(isVisible(cardElement.$.actionButton));

    // Click action button button and verify we navigated to checkup page and
    // started password checkup.
    cardElement.$.actionButton.click();
    assertEquals(Page.CHECKUP, Router.getInstance().currentRoute.page);
    assertEquals(
        'true',
        String(Router.getInstance().currentRoute.queryParameters.get(
            UrlParam.START_CHECK)));
    await flushTasks();

    // Verify that the notification card is hidden.
    cardElement = section.shadowRoot!.querySelector('notification-card');
    assertFalse(!!cardElement);
  });

  test('shortcut card', async function() {
    notificationCardsProxy.card = {
      id: 'password_shortcut_promo',
      title: 'Shortcut promo',
      description: 'Shortcut promo description.',
      actionButtonText: 'Add shortcut',
    };

    const section = await createPasswordsSection();
    let cardElement = section.shadowRoot!.querySelector('notification-card');

    // Verify notification card is shown.
    assertTrue(!!cardElement);
    assertTrue(isVisible(cardElement.$.actionButton));

    // Click action button button and verify we navigated to checkup page and
    // started password checkup.
    cardElement.$.actionButton.click();
    await passwordManager.whenCalled('showAddShortcutDialog');
    await flushTasks();

    // Verify that the notification card is hidden.
    cardElement = section.shadowRoot!.querySelector('notification-card');
    assertFalse(!!cardElement);
  });

  test('move passwords card hidden if no local passwords', async function() {
    notificationCardsProxy.card = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageActive = true;
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
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();
    const cardElement = section.shadowRoot!.querySelector('notification-card');
    assertFalse(!!cardElement);
  });

  test('move passwords card hidden if butter disabled', async function() {
    notificationCardsProxy.card = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageActive = false;
    passwordManager.data.groups = [createCredentialGroup({
      name: 'test.com',
      credentials: [createPasswordEntry(
          {username: 'user', id: 0, inProfileStore: true})],
    })];
    syncProxy.syncInfo = {
      isSyncingPasswords: false,
    };

    const section = await createPasswordsSection();
    const cardElement = section.shadowRoot!.querySelector('notification-card');
    assertFalse(!!cardElement);
  });

  test('move passwords card visible opens batch upload', async function() {
    notificationCardsProxy.card = {
      id: 'move_passwords_promo',
      title: 'Move passwords promo',
      description: 'Move passwords description.',
      actionButtonText: 'Move passwords',
    };
    passwordManager.data.isAccountStorageActive = true;

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
      isSyncingPasswords: false,
    };

    passwordManager.setRequestCredentialsDetailsResponse([password]);

    const section = await createPasswordsSection();
    const cardElement = section.shadowRoot!.querySelector('notification-card');
    assertTrue(!!cardElement);
    assertTrue(isVisible(cardElement.$.actionButton));

    cardElement.$.actionButton.click();
    await flushTasks();

    const entryPoint = await syncProxy.whenCalled('openBatchUpload');
    assertEquals(BatchUploadPasswordsEntryPoint.PROMO_CARD, entryPoint);
  });
});
