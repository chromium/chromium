// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, SyncBrowserProxyImpl} from 'chrome://password-manager/password_manager.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {TestSyncBrowserProxy} from './test_sync_browser_proxy.js';
import {createAffiliatedDomain, createPasswordEntry} from './test_util.js';

suite('AddPasswordDialogTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let syncProxy: TestSyncBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    syncProxy = new TestSyncBrowserProxy();
    SyncBrowserProxyImpl.setInstance(syncProxy);
    return flushTasks();
  });

  test('content correctly displayed', async function() {
    const password =
        createPasswordEntry({id: 0, username: 'user1', password: 'sTr0nGp@@s'});
    password.affiliatedDomains = [
      createAffiliatedDomain('test.com'),
      createAffiliatedDomain('m.test.com'),
    ];

    syncProxy.accountInfo = {
      email: 'test@gmail.com',
      avatarImage: 'chrome://image-url/',
    };

    const dialog = document.createElement('move-passwords-dialog');
    dialog.passwords = [password];
    document.body.appendChild(dialog);
    await flushTasks();

    assertEquals(
        syncProxy.accountInfo.email, dialog.$.accountEmail.textContent!.trim());
    assertEquals(syncProxy.accountInfo.avatarImage, dialog.$.avatar.src);
  });
});
