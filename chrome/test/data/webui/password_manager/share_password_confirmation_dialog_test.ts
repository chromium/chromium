// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import {PasswordManagerImpl, SharePasswordConfirmationDialogElement} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {makeRecipientInfo} from './test_util.js';

const SHARED_PASSWORD_ID = 42;

function assertVisibleTextContent(element: HTMLElement, expectedText: string) {
  assertTrue(isVisible(element));
  assertEquals(expectedText, element?.textContent!.trim());
}

suite('SharePasswordConfirmationDialogTest', function() {
  let dialog: SharePasswordConfirmationDialogElement;
  let passwordManager: TestPasswordManagerProxy;
  let mockTimer: MockTimer;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    mockTimer = new MockTimer();
    mockTimer.install();
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    dialog = document.createElement('share-password-confirmation-dialog');
    dialog.passwordId = SHARED_PASSWORD_ID;
    dialog.recipients = [makeRecipientInfo()];
    document.body.appendChild(dialog);
    flush();
  });

  teardown(function() {
    mockTimer.uninstall();
  });

  test('Has correct loading state', function() {
    assertVisibleTextContent(
        dialog.$.header, dialog.i18n('shareDialogLoadingTitle'));
    assertVisibleTextContent(dialog.$.cancel, dialog.i18n('cancel'));
    assertFalse(isVisible(dialog.$.done));
  });

  test('Has correct canceled state', function() {
    assertTrue(isVisible(dialog.$.cancel));
    dialog.$.cancel.click();
    flush();

    assertVisibleTextContent(
        dialog.$.header, dialog.i18n('shareDialogCanceledTitle'));
    assertVisibleTextContent(dialog.$.done, dialog.i18n('done'));
    assertFalse(isVisible(dialog.$.cancel));
  });

  test('Has correct success state', async function() {
    // The user has 5 seconds to cancel the dialog, after that `sharePassword`
    // is triggered.
    mockTimer.tick(5000);
    flush();

    assertVisibleTextContent(
        dialog.$.header, dialog.i18n('shareDialogSuccessTitle'));

    const [actualId, actualRecipients] =
        await passwordManager.whenCalled('sharePassword');
    assertEquals(SHARED_PASSWORD_ID, actualId);
    assertEquals(1, actualRecipients.length);
  });
});
