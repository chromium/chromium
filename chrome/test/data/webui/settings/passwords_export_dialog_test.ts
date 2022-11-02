// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Passwords Export Dialog tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {isChromeOS, isLacros} from 'chrome://resources/js/platform.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CrDialogElement, PasswordsExportDialogElement} from 'chrome://settings/lazy_load.js';
import {PasswordManagerImpl} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

import {PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

// Test that tapping "Export passwords" notifies the browser.
export async function runStartExportTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();
  await passwordManager.whenCalled('exportPasswords');
}

// Test the export flow. If exporting is fast, we should skip the
// in-progress view altogether.
export function runExportFlowFastTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const progressCallback =
      passwordManager.lastCallback.addPasswordsFileExportProgressListener!;

  // Use this to freeze the delayed progress bar and avoid flakiness.
  const mockTimer = new MockTimer();
  mockTimer.install();

  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();
  flush();
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});

  flush();
  // When we are done, the export dialog closes completely.
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_start'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_error'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_progress'));

  mockTimer.uninstall();
}

// The error view is shown when an error occurs.
export function runExportFlowErrorTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const progressCallback =
      passwordManager.lastCallback.addPasswordsFileExportProgressListener!;

  // Use this to freeze the delayed progress bar and avoid flakiness.
  const mockTimer = new MockTimer();
  mockTimer.install();

  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  progressCallback({
    status: chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
    folderName: 'tmp',
  });

  flush();
  // Test that the error dialog is shown.
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_error')!.open);
  // Test that the error dialog can be dismissed.
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#cancelErrorButton')!.click();
  flush();
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_error'));

  mockTimer.uninstall();
}

// The error view allows to retry.
export function runExportFlowErrorRetryTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const progressCallback =
      passwordManager.lastCallback.addPasswordsFileExportProgressListener!;
  // Use this to freeze the delayed progress bar and avoid flakiness.
  const mockTimer = new MockTimer();

  mockTimer.install();

  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();

  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  progressCallback({
    status: chrome.passwordsPrivate.ExportProgressStatus.FAILED_WRITE_FAILED,
    folderName: 'tmp',
  });

  flush();
  // Test that the error dialog is shown.
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_error')!.open);
  // Test that clicking retry will start a new export.

  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#tryAgainButton')!.click();

  mockTimer.uninstall();
}

// Test the export flow. If exporting is slow, Chrome should show the
// in-progress dialog for at least 1000ms.
export function runExportFlowSlowTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const progressCallback =
      passwordManager.lastCallback.addPasswordsFileExportProgressListener!;

  const mockTimer = new MockTimer();
  mockTimer.install();

  // The initial dialog remains open for 100ms after export enters the
  // in-progress state.
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();
  flush();
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  flush();
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);

  // After 100ms of not having completed, the dialog switches to the
  // progress bar. Chrome will continue to show the progress bar for 1000ms,
  // despite a completion event.
  mockTimer.tick(99);
  flush();
  assertTrue(
      exportDialog.shadowRoot!.querySelector<CrDialogElement>(
                                  '#dialog_start')!.open);
  mockTimer.tick(1);
  flush();
  assertTrue(exportDialog.shadowRoot!
                 .querySelector<CrDialogElement>('#dialog_progress')!.open);
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
  flush();
  assertTrue(exportDialog.shadowRoot!
                 .querySelector<CrDialogElement>('#dialog_progress')!.open);

  // After 1000ms, Chrome will display the completion event.
  mockTimer.tick(999);
  flush();
  assertTrue(exportDialog.shadowRoot!
                 .querySelector<CrDialogElement>('#dialog_progress')!.open);
  mockTimer.tick(1);
  flush();
  // On SUCCEEDED the dialog closes completely.
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_progress'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_start'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_error'));

  mockTimer.uninstall();
}

// Test that canceling the dialog while exporting will also cancel the
// export on the browser.
export async function runCancelExportTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  const progressCallback =
      passwordManager.lastCallback.addPasswordsFileExportProgressListener!;

  const mockTimer = new MockTimer();
  mockTimer.install();

  // The initial dialog remains open for 100ms after export enters the
  // in-progress state.
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();
  progressCallback(
      {status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  // The progress bar only appears after 100ms.
  mockTimer.tick(100);
  flush();
  assertTrue(exportDialog.shadowRoot!
                 .querySelector<CrDialogElement>('#dialog_progress')!.open);
  exportDialog.shadowRoot!
      .querySelector<HTMLElement>('#cancel_progress_button')!.click();
  await passwordManager.whenCalled('cancelExportPasswords');

  flush();
  // The dialog should be dismissed entirely.
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_progress'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_start'));
  assertFalse(!!exportDialog.shadowRoot!.querySelector<CrDialogElement>(
      '#dialog_error'));

  mockTimer.uninstall();
}

export async function runFireCloseEventAfterExportCompleteTest(
    exportDialog: PasswordsExportDialogElement,
    passwordManager: TestPasswordManagerProxy) {
  exportDialog.shadowRoot!.querySelector<HTMLElement>(
                              '#exportPasswordsButton')!.click();
  passwordManager.lastCallback.addPasswordsFileExportProgressListener!
      ({status: chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS});
  passwordManager.lastCallback.addPasswordsFileExportProgressListener!
      ({status: chrome.passwordsPrivate.ExportProgressStatus.SUCCEEDED});
  await eventToPromise('passwords-export-dialog-close', exportDialog);
}

suite('PasswordsExportDialog', function() {
  let passwordManager: TestPasswordManagerProxy;
  let elementFactory: PasswordSectionElementFactory;


  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    elementFactory = new PasswordSectionElementFactory(document);
  });


  if (!(isChromeOS || isLacros)) {
    // Test that tapping "Export passwords" notifies the browser.
    test('startExport', async function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      await runStartExportTest(exportDialog, passwordManager);
    });

    // Test the export flow. If exporting is fast, we should skip the
    // in-progress view altogether.
    test('exportFlowFast', function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      runExportFlowFastTest(exportDialog, passwordManager);
    });

    // The error view is shown when an error occurs.
    test('exportFlowError', function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      runExportFlowErrorTest(exportDialog, passwordManager);
    });

    // The error view allows to retry.
    test('exportFlowErrorRetry', function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      runExportFlowErrorRetryTest(exportDialog, passwordManager);
    });

    // Test the export flow. If exporting is slow, Chrome should show the
    // in-progress dialog for at least 1000ms.
    test('exportFlowSlow', function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      runExportFlowSlowTest(exportDialog, passwordManager);
    });

    // Test that canceling the dialog while exporting will also cancel the
    // export on the browser.
    test('cancelExport', async function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      await runCancelExportTest(exportDialog, passwordManager);
    });

    test('fires close event after export complete', async function() {
      const exportDialog = elementFactory.createExportPasswordsDialog();
      await runFireCloseEventAfterExportCompleteTest(
          exportDialog, passwordManager);
    });
  }

  // The export dialog is dismissable.
  test('exportDismissable', function() {
    const exportDialog = elementFactory.createExportPasswordsDialog();

    assertTrue(exportDialog.shadowRoot!
                   .querySelector<CrDialogElement>('#dialog_start')!.open);
    exportDialog.shadowRoot!.querySelector<HTMLElement>(
                                '#cancelButton')!.click();
    flush();
    assertFalse(!!exportDialog.shadowRoot!.querySelector('#dialog_start'));
  });

  test('fires close event when canceled', async function() {
    const exportDialog = elementFactory.createExportPasswordsDialog();
    exportDialog.shadowRoot!.querySelector<HTMLElement>(
                                '#cancelButton')!.click();
    await eventToPromise('passwords-export-dialog-close', exportDialog);
  });
});
