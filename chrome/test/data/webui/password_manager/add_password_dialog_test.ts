// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {Page, PASSWORD_NOTE_MAX_CHARACTER_COUNT, PasswordManagerImpl, Router, SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import type {MetricsTracker} from 'chrome://webui-test/metrics_test_support.js';
import {fakeMetricsPrivate} from 'chrome://webui-test/metrics_test_support.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('AddPasswordDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;
  let metricsTracker: MetricsTracker;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metricsTracker = fakeMetricsPrivate();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    return flushTasks();
  });

  test('url validation works', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
    assertFalse(isVisible(dialog.$.storePicker));
    assertFalse(dialog.$.websiteInput.invalid);

    // Make url invalid
    dialog.$.websiteInput.value = 'abc';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals('abc', await passwordManager.whenCalled('getUrlCollection'));
    await flushTasks();

    assertTrue(dialog.$.websiteInput.invalid);
    assertEquals(
        dialog.i18n('notValidWebsite'), dialog.$.websiteInput.errorMessage);

    // Now make URL valid again
    passwordManager.reset();
    dialog.$.websiteInput.value = 'www';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals('www', await passwordManager.whenCalled('getUrlCollection'));
    await flushTasks();
    assertFalse(dialog.$.websiteInput.invalid);

    // But after losing focus url is no longer valid due to missing '.'
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    await dialog.$.websiteInput.updateComplete;
    assertTrue(dialog.$.websiteInput.invalid);
    assertEquals(
        dialog.i18n('missingTLD', 'www.com'),
        dialog.$.websiteInput.errorMessage);
    assertTrue(dialog.$.addButton.disabled);
  });

  test('username validation works', async function() {
    passwordManager.data.passwords = [
      createPasswordEntry({url: 'www.example.com', username: 'test'}),
      createPasswordEntry({url: 'www.example2.com', username: 'test2'}),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];
    passwordManager.data.passwords[1]!.affiliatedDomains =
        [createAffiliatedDomain('www.example2.com')];

    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
    assertFalse(isVisible(dialog.$.storePicker));

    // Enter website for which user has a saved password.
    dialog.$.websiteInput.value = 'www.example.com';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals(
        'www.example.com',
        await passwordManager.whenCalled('getUrlCollection'));
    assertFalse(dialog.$.usernameInput.invalid);
    assertTrue(dialog.$.viewExistingPasswordLink.hidden);

    // Update username to the same value and observe error.
    dialog.$.usernameInput.value = 'test';
    await dialog.$.usernameInput.updateComplete;
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example.com'),
        dialog.$.usernameInput.errorMessage);
    assertFalse(dialog.$.viewExistingPasswordLink.hidden);

    // Update username and observe no error.
    dialog.$.usernameInput.value = 'test2';
    await dialog.$.usernameInput.updateComplete;
    assertFalse(dialog.$.usernameInput.invalid);
    assertTrue(dialog.$.viewExistingPasswordLink.hidden);

    // Update website input to match a second existing password and observe
    // error again.
    passwordManager.reset();
    dialog.$.websiteInput.value = 'www.example2.com';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals(
        'www.example2.com',
        await passwordManager.whenCalled('getUrlCollection'));
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example2.com'),
        dialog.$.usernameInput.errorMessage);
    assertFalse(dialog.$.viewExistingPasswordLink.hidden);
    assertTrue(dialog.$.addButton.disabled);
  });

  test('show/hide password', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
    assertFalse(isVisible(dialog.$.storePicker));

    assertEquals(
        dialog.i18n('showPassword'), dialog.$.showPasswordButton.title);
    assertEquals('password', dialog.$.passwordInput.type);
    assertTrue(dialog.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility', dialog.$.showPasswordButton.getAttribute('class'));

    dialog.$.showPasswordButton.click();

    assertEquals(
        dialog.i18n('hidePassword'), dialog.$.showPasswordButton.title);
    assertEquals('text', dialog.$.passwordInput.type);
    assertTrue(dialog.$.showPasswordButton.hasAttribute('class'));
    assertEquals(
        'icon-visibility-off',
        dialog.$.showPasswordButton.getAttribute('class'));
  });

  test('note validation works', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
    assertFalse(isVisible(dialog.$.storePicker));
    assertFalse(dialog.$.noteInput.invalid);

    // Make note 899 characters long.
    dialog.$.noteInput.value = '.'.repeat(899);
    assertFalse(dialog.$.noteInput.invalid);
    assertEquals('', dialog.$.noteInput.firstFooter);
    assertEquals('', dialog.$.noteInput.secondFooter);

    // After 900 characters there are footers.
    dialog.$.noteInput.value = '.'.repeat(900);
    await flushTasks();
    assertFalse(dialog.$.noteInput.invalid);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.noteInput.firstFooter);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCount', 900,
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.noteInput.secondFooter);

    // After PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1 characters note is no longer
    // valid.
    dialog.$.noteInput.value =
        '.'.repeat(PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1);
    await flushTasks();
    assertTrue(dialog.$.noteInput.invalid);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCountWarning',
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.noteInput.firstFooter);
    assertEquals(
        dialog.i18n(
            'passwordNoteCharacterCount', PASSWORD_NOTE_MAX_CHARACTER_COUNT + 1,
            PASSWORD_NOTE_MAX_CHARACTER_COUNT),
        dialog.$.noteInput.secondFooter);
    assertTrue(dialog.$.addButton.disabled);
  });

  test('password is saved', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
    assertFalse(isVisible(dialog.$.storePicker));

    // Enter website
    dialog.$.websiteInput.value = 'www.example.com';
    dialog.$.usernameInput.value = 'test';
    dialog.$.passwordInput.value = 'lastPass';
    dialog.$.noteInput.value = 'secret note.';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));

    await passwordManager.whenCalled('getUrlCollection');

    assertFalse(dialog.$.addButton.disabled);
    dialog.$.addButton.click();

    assertEquals(
        1,
        metricsTracker.count(
            'PasswordManager.PasswordNoteActionInSettings2',
            /*NOTE_ADDED_IN_ADD_DIALOG*/ 0));

    const params = await passwordManager.whenCalled('addPassword');

    assertEquals('https://www.example.com/login', params.url);
    assertEquals(dialog.$.usernameInput.value, params.username);
    assertEquals(dialog.$.passwordInput.value, params.password);
    assertEquals(dialog.$.noteInput.value, params.note);
    assertEquals(false, params.useAccountStore);
  });

  test('view saved password', async function() {
    Router.getInstance().navigateTo(Page.PASSWORDS);
    passwordManager.data.passwords = [
      createPasswordEntry({url: 'www.example.com', username: 'test'}),
    ];
    passwordManager.data.passwords[0]!.affiliatedDomains =
        [createAffiliatedDomain('www.example.com')];

    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();
    assertFalse(isVisible(dialog.$.storePicker));

    // Enter website
    dialog.$.websiteInput.value = 'www.example.com';
    dialog.$.usernameInput.value = 'test';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    await passwordManager.whenCalled('getUrlCollection');

    assertTrue(dialog.$.usernameInput.invalid);
    assertFalse(dialog.$.viewExistingPasswordLink.hidden);

    dialog.$.viewExistingPasswordLink.click();
    assertEquals(Page.PASSWORD_DETAILS, Router.getInstance().currentRoute.page);
    assertEquals(
        dialog.$.websiteInput.value, Router.getInstance().currentRoute.details);
  });

  test('account picker shows preferred storage account', async function() {
    passwordManager.data.isAccountStorageEnabled = true;
    passwordManager.data.isAccountStorageDefault = true;
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    assertTrue(isVisible(dialog.$.storePicker));
    assertEquals(
        chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT,
        dialog.$.storePicker.value);
  });

  test('account picker shows preferred storage device', async function() {
    passwordManager.data.isAccountStorageEnabled = true;
    passwordManager.data.isAccountStorageDefault = false;
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    assertTrue(isVisible(dialog.$.storePicker));
    assertEquals(
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE,
        dialog.$.storePicker.value);
  });

  test('save to account', async function() {
    passwordManager.data.isAccountStorageEnabled = true;
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.$.storePicker.value =
        chrome.passwordsPrivate.PasswordStoreSet.ACCOUNT;

    // Enter website
    dialog.$.websiteInput.value = 'www.example.com';
    dialog.$.usernameInput.value = 'test';
    dialog.$.passwordInput.value = 'lastPass';
    dialog.$.noteInput.value = 'secret note.';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));

    await Promise.all([
      passwordManager.whenCalled('getUrlCollection'),
      dialog.$.usernameInput.updateComplete,
      dialog.$.passwordInput.updateComplete,
    ]);

    assertFalse(dialog.$.addButton.disabled);
    dialog.$.addButton.click();

    const params = await passwordManager.whenCalled('addPassword');

    assertEquals('https://www.example.com/login', params.url);
    assertEquals(dialog.$.usernameInput.value, params.username);
    assertEquals(dialog.$.passwordInput.value, params.password);
    assertEquals(dialog.$.noteInput.value, params.note);
    assertEquals(true, params.useAccountStore);
  });

  test('save to device', async function() {
    passwordManager.data.isAccountStorageEnabled = true;
    syncProxy.syncInfo = {
      isEligibleForAccountStorage: true,
      isSyncingPasswords: false,
    };

    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    dialog.$.storePicker.value =
        chrome.passwordsPrivate.PasswordStoreSet.DEVICE;

    // Enter website
    dialog.$.websiteInput.value = 'www.example.com';
    dialog.$.usernameInput.value = 'test';
    dialog.$.passwordInput.value = 'lastPass';
    dialog.$.noteInput.value = 'secret note.';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));

    await Promise.all([
      passwordManager.whenCalled('getUrlCollection'),
      dialog.$.usernameInput.updateComplete,
      dialog.$.passwordInput.updateComplete,
    ]);

    assertFalse(dialog.$.addButton.disabled);
    dialog.$.addButton.click();

    const params = await passwordManager.whenCalled('addPassword');

    assertEquals('https://www.example.com/login', params.url);
    assertEquals(dialog.$.usernameInput.value, params.username);
    assertEquals(dialog.$.passwordInput.value, params.password);
    assertEquals(dialog.$.noteInput.value, params.note);
    assertEquals(false, params.useAccountStore);
  });

  test('error when leaving website blank', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    assertFalse(dialog.$.websiteInput.invalid);
    assertFalse(dialog.$.websiteInput.hasAttribute('show-error-message'));
    assertEquals(null, dialog.$.websiteInput.errorMessage);
    // Simulate losing focus.
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    await dialog.$.websiteInput.updateComplete;

    assertTrue(dialog.$.websiteInput.invalid);
    assertFalse(dialog.$.websiteInput.hasAttribute('show-error-message'));
    assertEquals('', dialog.$.websiteInput.errorMessage);

    // Simulate losing focus.
    dialog.$.websiteInput.value = 'abc';
    await dialog.$.websiteInput.updateComplete;
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    await dialog.$.websiteInput.updateComplete;

    assertTrue(dialog.$.websiteInput.invalid);
    assertTrue(dialog.$.websiteInput.hasAttribute('show-error-message'));
    assertEquals(
        dialog.i18n('missingTLD', 'abc.com'),
        dialog.$.websiteInput.errorMessage);
  });

  test('error when leaving password blank', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    assertFalse(dialog.$.passwordInput.invalid);

    dialog.$.passwordInput.dispatchEvent(new CustomEvent('blur'));
    await dialog.$.websiteInput.updateComplete;
    await flushTasks();

    assertTrue(dialog.$.passwordInput.invalid);
  });
});
