// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Import Dialog tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PasswordManagerImpl} from 'chrome://settings/settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

suite('PasswordsImportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;
  let elementFactory: PasswordSectionElementFactory;

  setup(function() {
    document.body.innerHTML = '';
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    elementFactory = new PasswordSectionElementFactory(document);
  });

  test('hasCorrectInitialState', function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertTrue(isVisible(importDialog.$.cancel));
    assertTrue(isVisible(importDialog.$.chooseFile));
    assertEquals(
        loadTimeData.getString('cancel'),
        importDialog.$.cancel.textContent!.trim());
    assertEquals(
        loadTimeData.getString('importPasswordsChooseFile'),
        importDialog.$.chooseFile.textContent!.trim());
  });

  test('triggersImportWithChooseFileButton', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertTrue(isVisible(importDialog.$.chooseFile));

    importDialog.$.chooseFile.click();
    await passwordManager.whenCalled('importPasswords');
    await eventToPromise('close', importDialog);
  });

  test('cancelButtonClosesDialog', async function() {
    const importDialog = elementFactory.createPasswordsImportDialog();
    assertTrue(isVisible(importDialog.$.cancel));

    importDialog.$.cancel.click();
    await eventToPromise('close', importDialog);
  });
});