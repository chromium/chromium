// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CrButtonElement, CrDialogElement, PasswordsExporterElement} from 'chrome://password-manager/password_manager.js';
import {PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {MockTimer} from 'chrome://webui-test/mock_timer.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

const ExportProgressStatus = chrome.passwordsPrivate.ExportProgressStatus;

// Checks the expected display of elements on PasswordsExporterElement creation.
function verifyInitialUIState(exporter: PasswordsExporterElement) {
  assertTrue(!!exporter.shadowRoot!.querySelector('#exportPasswordsButton'));
  assertFalse(!!exporter.shadowRoot!.querySelector('#progressSpinner'));
  assertFalse(!!exporter.shadowRoot!.querySelector('#dialogError'));
}

function clickExportPasswordsButton(exporter: PasswordsExporterElement) {
  const exportPassswordsButton =
      exporter.shadowRoot!.querySelector<HTMLElement>('#exportPasswordsButton');
  assertTrue(!!exportPassswordsButton);
  exportPassswordsButton.click();
  flush();
}

function updateExportStatus(
    passwordManager: TestPasswordManagerProxy,
    progress: chrome.passwordsPrivate.PasswordExportProgress) {
  const progressCallback =
      passwordManager.listeners.passwordsFileExportProgressListener;
  assertTrue(!!progressCallback);
  progressCallback(progress);
}

suite('PasswordExporterTest', function() {
  let passwordManager: TestPasswordManagerProxy;
  let passwordsExporter: PasswordsExporterElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);

    passwordsExporter = document.createElement('passwords-exporter');
    document.body.appendChild(passwordsExporter);
    flush();
    verifyInitialUIState(passwordsExporter);
  });

  // Test the export flow. Tapping "Export passwords" notifies the browser
  // on start and skips the in-progress view altogether, if the exporting
  // is fast.
  test('successfulExportFlow', async function() {
    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    updateExportStatus(
        passwordManager, {status: ExportProgressStatus.IN_PROGRESS});
    flush();
    assertTrue(
        !!passwordsExporter.shadowRoot!.querySelector('#progressSpinner'));

    const successToast = passwordsExporter.$.exportSuccessToast;
    assertFalse(successToast.open);

    updateExportStatus(passwordManager, {
      status: ExportProgressStatus.SUCCEEDED,
      filePath: 'usr/testfolder/testfile.csv',
    });
    flush();
    // On SUCCEEDED, the exporter UI is back to the initial state and the
    // success toast is shown.
    verifyInitialUIState(passwordsExporter);
    assertTrue(successToast.open);

    const openInShellButton =
        successToast.querySelector<CrButtonElement>('#openInShellButton');
    assertTrue(!!openInShellButton);
    openInShellButton.click();
    await passwordManager.whenCalled('showExportedFileInShell');
  });

  // The error view is shown when an error occurs.
  test('exportFlowError', async function() {
    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    updateExportStatus(
        passwordManager, {status: ExportProgressStatus.IN_PROGRESS});
    updateExportStatus(passwordManager, {
      status: ExportProgressStatus.FAILED_WRITE_FAILED,
      folderName: 'tmp',
    });
    flush();
    const errorDialog =
        passwordsExporter.shadowRoot!.querySelector<CrDialogElement>(
            '#dialogError');
    assertTrue(!!errorDialog);

    // Test that the error dialog can be dismissed.
    const cancelButton =
        errorDialog.querySelector<CrButtonElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    flush();

    verifyInitialUIState(passwordsExporter);
  });

  // The error view allows to retry.
  test('exportFlowErrorRetry', async function() {
    // Use this to freeze the delayed progress spinner and avoid flakiness.
    const mockTimer = new MockTimer();
    mockTimer.install();

    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    updateExportStatus(
        passwordManager, {status: ExportProgressStatus.IN_PROGRESS});
    updateExportStatus(passwordManager, {
      status: ExportProgressStatus.FAILED_WRITE_FAILED,
      folderName: 'tmp',
    });
    flush();
    // Test that the error dialog is shown.
    const errorDialog =
        passwordsExporter.shadowRoot!.querySelector<CrDialogElement>(
            '#dialogError');
    assertTrue(!!errorDialog);

    // Test that clicking retry will start a new export.
    const tryAgainButton =
        errorDialog.querySelector<CrButtonElement>('#tryAgainButton');
    assertTrue(!!tryAgainButton);
    tryAgainButton.click();
    flush();

    await passwordManager.whenCalled('requestExportProgressStatus');
  });
});