// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

suite('PasswordExportTest', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  test('Export dialog appears after clicking on button', async function() {
    const exportElement = document.createElement('password-exporter');
    document.body.appendChild(exportElement);
    await flushTasks();

    exportElement.$.exportPasswordsButton.click();
    await passwordManager.whenCalled('exportPasswords');
    const exportPasswordsDialog =
        exportElement.shadowRoot!.querySelector('passwords-export-dialog');
    assertTrue(!!exportPasswordsDialog);
  });
});