// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Export Dialog tests. */

// clang-format off
import 'chrome://password-manager/password_manager.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement, PasswordsExportDialogElement, PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
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

function assertDialogIsShown(
    exportDialogElement: PasswordsExportDialogElement, dialogId: string) {
  const dialog =
      exportDialogElement.shadowRoot!.querySelector<CrDialogElement>(dialogId);
  assertTrue(!!dialog);
  assertTrue(dialog.open);
}

function checkThatNoDialogIsShown(exportDialog: PasswordsExportDialogElement) {
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialogProgress'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialogError'));
}

function updateExportStatus(
    passwordManager: TestPasswordManagerProxy,
    progress: chrome.passwordsPrivate.PasswordExportProgress) {
  const progressCallback =
      passwordManager.listeners.passwordsFileExportProgressListener;
  assertTrue(!!progressCallback);
  progressCallback(progress);
}

suite('PasswordsExportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
  });

  // Test that tapping "Export passwords" notifies the browser on start and
  // fires close event on completion.
  test('Export starts and finishes', async function() {
    const exportDialog = createExportPasswordsDialog();

    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
    await eventToPromise('passwords-export-dialog-close', exportDialog);
  });

  // Test the export flow. If exporting is fast, we should skip the
  // in-progress view altogether.
  test('exportFlowFast', function() {
    const exportDialog = createExportPasswordsDialog();

    // Use this to freeze the delayed progress bar and avoid flakiness.
    const mockTimer = new MockTimer();
    mockTimer.install();

    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});

    flush();
    // When we are done, the export dialog closes completely.
    checkThatNoDialogIsShown(exportDialog);
  });

  // Test the export flow. If exporting is slow, Chrome should show the
  // in-progress dialog for at least 1000ms.
  test('exportFlowSlow', function() {
    const exportDialog = createExportPasswordsDialog();

    const mockTimer = new MockTimer();
    mockTimer.install();

    // No progress dialog is shown for 100ms after export enters the
    // in-progress state.
    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
    flush();
    checkThatNoDialogIsShown(exportDialog);

    // After 100ms of not having completed, the progress bar is shown. Chrome
    // will continue to show the progress bar for 1000ms, despite a completion
    // event.
    mockTimer.tick(99);
    flush();
    checkThatNoDialogIsShown(exportDialog);

    mockTimer.tick(1);
    flush();
    assertDialogIsShown(exportDialog, '#dialogProgress');
    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
    flush();
    assertDialogIsShown(exportDialog, '#dialogProgress');

    // Until 1000ms expire, progress dialog is shown, even though
    // export has already finished.
    mockTimer.tick(999);
    flush();
    assertDialogIsShown(exportDialog, '#dialogProgress');
    mockTimer.tick(1);
    flush();
    // On SUCCEEDED the dialog closes completely.
    checkThatNoDialogIsShown(exportDialog);
  });

  // Test that canceling the dialog while exporting will also cancel the
  // export on the browser.
  test('cancelExport', async function() {
    const exportDialog = createExportPasswordsDialog();

    const mockTimer = new MockTimer();
    mockTimer.install();

    // No progress dialog is shown for 100ms after export enters the
    // in-progress state.
    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
    checkThatNoDialogIsShown(exportDialog);
    // The progress bar only appears after 100ms.
    mockTimer.tick(100);
    flush();
    assertDialogIsShown(exportDialog, '#dialogProgress');

    const cancelProgressButton =
        exportDialog.shadowRoot!.querySelector<HTMLElement>(
            '#cancelProgressButton');
    assertTrue(!!cancelProgressButton);
    cancelProgressButton.click();
    await passwordManager.whenCalled('cancelExportPasswords');

    flush();
    // The dialog should be dismissed entirely.
    checkThatNoDialogIsShown(exportDialog);
  });

  // The error view is shown when an error occurs.
  test('exportFlowError', function() {
    const exportDialog = createExportPasswordsDialog();

    // Use this to freeze the delayed progress bar and avoid flakiness.
    const mockTimer = new MockTimer();
    mockTimer.install();

    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
    updateExportStatus(passwordManager, {
      status: chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
      folderName: 'tmp',
    });
    flush();
    assertDialogIsShown(exportDialog, '#dialogError');

    // Test that the error dialog can be dismissed.
    const cancelErrorButton =
        exportDialog.shadowRoot!.querySelector<HTMLElement>(
            '#cancelErrorButton');
    assertTrue(!!cancelErrorButton);
    cancelErrorButton.click();
    flush();

    checkThatNoDialogIsShown(exportDialog);
  });

  // The error view allows to retry.
  test('exportFlowErrorRetry', async function() {
    const exportDialog = createExportPasswordsDialog();

    // Use this to freeze the delayed progress bar and avoid flakiness.
    const mockTimer = new MockTimer();
    mockTimer.install();

    updateExportStatus(
        passwordManager,
        {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});

    updateExportStatus(passwordManager, {
      status: chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
      folderName: 'tmp',
    });
    flush();
    // Test that the error dialog is shown.
    assertDialogIsShown(exportDialog, '#dialogError');

    // Test that clicking retry will start a new export.
    const tryAgainButton =
        exportDialog.shadowRoot!.querySelector<HTMLElement>('#tryAgainButton');
    assertTrue(!!tryAgainButton);
    tryAgainButton.click();
    await passwordManager.whenCalled('requestExportProgressStatus');
  });
});