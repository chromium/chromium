// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

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
});
