// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {SharePasswordConfirmationDialogElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';
import {createPasswordEntry, makeRecipientInfo} from './test_util.js';

const SHARED_PASSWORD_ID = 42;
const SHARED_PASSWORD_NAME = 'example.com';
const HTTPS_CHANGE_PASSWORD_URL = 'https://example.com';
const HTTP_CHANGE_PASSWORD_URL = 'http://example.com';

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
    dialog.passwordName = SHARED_PASSWORD_NAME;
    dialog.password = createPasswordEntry({
      id: SHARED_PASSWORD_ID,
      changePasswordUrl: HTTPS_CHANGE_PASSWORD_URL,
    });
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
    assertFalse(isVisible(dialog.$.description));
    assertFalse(isVisible(dialog.$.footerDescription));
  });

  test('Has correct canceled state', function() {
    assertTrue(isVisible(dialog.$.cancel));
    dialog.$.cancel.click();
    flush();

    assertVisibleTextContent(
        dialog.$.header, dialog.i18n('shareDialogCanceledTitle'));
    assertVisibleTextContent(dialog.$.done, dialog.i18n('done'));
    assertFalse(isVisible(dialog.$.cancel));
    assertFalse(isVisible(dialog.$.description));
    assertFalse(isVisible(dialog.$.footerDescription));
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

  test(
      'Has correct success state with single recipient (https site)',
      async function() {
        // The user has 5 seconds to cancel the dialog, after that
        // `sharePassword` is triggered.
        mockTimer.tick(5000);
        flush();

        assertVisibleTextContent(
            dialog.$.header, dialog.i18n('shareDialogSuccessTitle'));

        assertTrue(isVisible(dialog.$.description));
        assertEquals(
            dialog.$.description.innerHTML,
            dialog
                .i18nAdvanced(
                    'sharePasswordConfirmationDescriptionSingleRecipient', {
                      substitutions: [
                        'New User',
                        SHARED_PASSWORD_NAME,
                      ],
                    })
                .toString());
        assertTrue(isVisible(dialog.$.footerDescription));
        assertEquals(
            dialog.$.footerDescription.innerHTML,
            dialog
                .i18nAdvanced('sharePasswordConfirmationFooterWebsite', {
                  substitutions: [
                    `<a href='${HTTPS_CHANGE_PASSWORD_URL}' target='_blank'>${
                        SHARED_PASSWORD_NAME}</a>`,
                  ],
                })
                .toString());

        const [actualId, actualRecipients] =
            await passwordManager.whenCalled('sharePassword');
        assertEquals(SHARED_PASSWORD_ID, actualId);
        assertEquals(1, actualRecipients.length);
      });

  test(
      'Has correct success footer with single recipient (http site)',
      async function() {
        dialog.password = createPasswordEntry({
          id: SHARED_PASSWORD_ID,
          changePasswordUrl: HTTP_CHANGE_PASSWORD_URL,
        });

        // The user has 5 seconds to cancel the dialog, after that
        // `sharePassword` is triggered.
        mockTimer.tick(5000);
        flush();

        assertEquals(
            dialog.$.footerDescription.innerHTML,
            dialog
                .i18nAdvanced('sharePasswordConfirmationFooterWebsite', {
                  substitutions: [
                    SHARED_PASSWORD_NAME,
                  ],
                })
                .toString());

        const [actualId, actualRecipients] =
            await passwordManager.whenCalled('sharePassword');
        assertEquals(SHARED_PASSWORD_ID, actualId);
        assertEquals(1, actualRecipients.length);
      });

  test('Has correct footer for shared Android creadential', async function() {
    // Credential without change password url is an Android credentail.
    dialog.password = createPasswordEntry({
      id: SHARED_PASSWORD_ID,
    });
    // The user has 5 seconds to cancel the dialog, after that `sharePassword`
    // is triggered.
    mockTimer.tick(5000);
    flush();

    assertTrue(isVisible(dialog.$.footerDescription));
    assertEquals(
        dialog.$.footerDescription.innerHTML,
        dialog.i18nAdvanced('sharePasswordConfirmationFooterAndroidApp')
            .toString());

    await passwordManager.whenCalled('sharePassword');
  });

  test(
      'Has correct success description for multiple recipients',
      async function() {
        dialog.recipients = [makeRecipientInfo(), makeRecipientInfo()];
        // The user has 5 seconds to cancel the dialog, after that
        // `sharePassword` is triggered.
        mockTimer.tick(5000);
        flush();

        assertVisibleTextContent(
            dialog.$.header, dialog.i18n('shareDialogSuccessTitle'));

        assertTrue(isVisible(dialog.$.description));
        assertEquals(
            dialog.$.description.innerHTML,
            dialog
                .i18nAdvanced(
                    'sharePasswordConfirmationDescriptionMultipleRecipients', {
                      substitutions: [
                        SHARED_PASSWORD_NAME,
                      ],
                    })
                .toString());

        const [actualId, actualRecipients] =
            await passwordManager.whenCalled('sharePassword');
        assertEquals(SHARED_PASSWORD_ID, actualId);
        assertEquals(2, actualRecipients.length);
      });
});
