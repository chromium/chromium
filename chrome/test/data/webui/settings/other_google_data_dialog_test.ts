// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {SettingsOtherGoogleDataDialogElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl, PasswordManagerPage} from 'chrome://settings/settings.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// TODO(crbug.com/422340428): Add tests for back & cancel buttons.
suite('OtherGoogleDataDialog', function() {
  let dialog: SettingsOtherGoogleDataDialogElement;
  let passwordManagerProxy: TestPasswordManagerProxy;

  setup(function() {
    passwordManagerProxy = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManagerProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dialog = document.createElement('settings-other-google-data-dialog');
    document.body.appendChild(dialog);
    return flushTasks();
  });

  test('PasswordManagerLinkClick', async function() {
    dialog.$.passwordManagerLink.click();

    assertEquals(
        PasswordManagerPage.PASSWORDS,
        await passwordManagerProxy.whenCalled('showPasswordManager'));
  });
});
