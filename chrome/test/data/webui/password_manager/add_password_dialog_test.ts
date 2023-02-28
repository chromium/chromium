// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('AddPasswordDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    return flushTasks();
  });

  test('url validation works', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

    assertFalse(dialog.$.websiteInput.invalid);

    // Make url invalid
    dialog.$.websiteInput.value = 'abc';
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals('abc', await passwordManager.whenCalled('getUrlCollection'));
    await flushTasks();

    assertTrue(dialog.$.websiteInput.invalid);
    assertEquals(
        dialog.i18n('notValidWebsite'), dialog.$.websiteInput.errorMessage);

    // Now make URL valid again
    passwordManager.reset();
    dialog.$.websiteInput.value = 'www';
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals('www', await passwordManager.whenCalled('getUrlCollection'));
    await flushTasks();
    assertFalse(dialog.$.websiteInput.invalid);

    // But after losing focus url is no longer valid due to missing '.'
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    assertTrue(dialog.$.websiteInput.invalid);
    assertEquals(
        dialog.i18n('missingTLD', 'www.com'),
        dialog.$.websiteInput.errorMessage);
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

    // Enter website for which user has a saved password.
    dialog.$.websiteInput.value = 'www.example.com';
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals(
        'www.example.com',
        await passwordManager.whenCalled('getUrlCollection'));
    assertFalse(dialog.$.usernameInput.invalid);

    // Update username to the same value and observe error.
    dialog.$.usernameInput.value = 'test';
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example.com'),
        dialog.$.usernameInput.errorMessage);

    // Update username and observe no error.
    dialog.$.usernameInput.value = 'test2';
    assertFalse(dialog.$.usernameInput.invalid);

    // Update website input to match a second existing password and observe
    // error again.
    passwordManager.reset();
    dialog.$.websiteInput.value = 'www.example2.com';
    dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));
    assertEquals(
        'www.example2.com',
        await passwordManager.whenCalled('getUrlCollection'));
    assertTrue(dialog.$.usernameInput.invalid);
    assertEquals(
        dialog.i18n('usernameAlreadyUsed', 'www.example2.com'),
        dialog.$.usernameInput.errorMessage);
  });

  test('show/hide password', async function() {
    const dialog = document.createElement('add-password-dialog');
    document.body.appendChild(dialog);
    await flushTasks();

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
});
