// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Export Dialog tests. */

// clang-format off
import 'chrome://password-manager/password_manager.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement, PasswordsExportDialogElement, PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

/**
 * Helper method used to create an export passwords dialog.
 */
function createExportPasswordsDialog(): PasswordsExportDialogElement {
  const dialog = document.createElement('passwords-export-dialog');
  document.body.appendChild(dialog);
  flush();
  return dialog;
}

suite('PasswordsExportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  // The export dialog is dismissable.
  test('exportDismissable', async function() {
    const exportDialog = createExportPasswordsDialog();
    await passwordManager.whenCalled('requestExportProgressStatus');
    const dialogStart =
        exportDialog.shadowRoot!.querySelector<CrDialogElement>('#dialogStart');
    assertTrue(!!dialogStart);
    assertTrue(dialogStart.open);

    const cancelButton =
        exportDialog.shadowRoot!.querySelector<HTMLElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    flush();
    assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
        '#dialogStart'));
  });

  test('fires close event when canceled', async function() {
    const exportDialog = createExportPasswordsDialog();
    await passwordManager.whenCalled('requestExportProgressStatus');
    const cancelButton =
        exportDialog.shadowRoot!.querySelector<HTMLElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await eventToPromise('passwords-export-dialog-close', exportDialog);
  });

  // Test that tapping "Export passwords" notifies the browser on start and
  // fires close event on completion.
  test('Export starts and finishes', async function() {
    const exportDialog = createExportPasswordsDialog();
    const exportButton = exportDialog.shadowRoot!.querySelector<HTMLElement>(
        '#exportPasswordsButton');
    assertTrue(!!exportButton);
    exportButton.click();
    await passwordManager.whenCalled('exportPasswords');

    const progressCallback =
        passwordManager.listeners.passwordsFileExportProgressListener;
    assertTrue(!!progressCallback);
    progressCallback(
        {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
    await eventToPromise('passwords-export-dialog-close', exportDialog);
  });
});