// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://password-manager/password_manager.js';

import type {CrButtonElement, CrDialogElement, PasswordsExporterElement} from 'chrome://password-manager/password_manager.js';
import {ExportPasswordsResult, ExportProgressStatus, PasswordManagerImpl} from 'chrome://password-manager/password_manager.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// Checks the expected display of elements on PasswordsExporterElement creation.
function verifyInitialUIState(exporter: PasswordsExporterElement) {
  assertTrue(!!exporter.shadowRoot.querySelector('#exportPasswordsButton'));
  assertFalse(isVisible(exporter.shadowRoot.querySelector('.spinner')));
  assertFalse(!!exporter.shadowRoot.querySelector('#dialogError'));
}

function clickExportPasswordsButton(exporter: PasswordsExporterElement) {
  const exportPassswordsButton =
      exporter.shadowRoot.querySelector<HTMLElement>('#exportPasswordsButton');
  assertTrue(!!exportPassswordsButton);
  exportPassswordsButton.click();
}

async function updateExportStatus(
    passwordManager: TestPasswordManagerProxy, status: ExportProgressStatus,
    folderName?: string) {
  passwordManager.callbackRouterRemote.onPasswordsExportProgress(
      status, folderName || null);
  await passwordManager.callbackRouterRemote.$.flushForTesting();
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
    verifyInitialUIState(passwordsExporter);
  });

  // Test the export flow. Tapping "Export passwords" notifies the browser
  // on start and skips the in-progress view altogether, if the exporting
  // is fast.
  test('successfulExportFlow', async function() {
    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    await updateExportStatus(passwordManager, ExportProgressStatus.kInProgress);
    await microtasksFinished();
    assertTrue(
        isVisible(passwordsExporter.shadowRoot.querySelector('.spinner')));

    const successToast = passwordsExporter.$.exportSuccessToast;
    assertFalse(successToast.open);

    await updateExportStatus(passwordManager, ExportProgressStatus.kSucceeded);
    await microtasksFinished();
    // On SUCCEEDED, the exporter UI is back to the initial state and the
    // success toast is shown.
    verifyInitialUIState(passwordsExporter);
    assertTrue(successToast.open);

    const openInShellButton =
        successToast.querySelector<CrButtonElement>('#openInShellButton');
    assertTrue(!!openInShellButton);
    openInShellButton.click();
    await passwordManager.whenCalled('showLastExportedFileInShell');
  });

  // The error view is shown when an error occurs.
  test('exportFlowError', async function() {
    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    await updateExportStatus(passwordManager, ExportProgressStatus.kInProgress);
    await updateExportStatus(
        passwordManager, ExportProgressStatus.kFailedWrite, 'tmp');
    await microtasksFinished();
    const errorDialog =
        passwordsExporter.shadowRoot.querySelector<CrDialogElement>(
            '#dialogError');
    assertTrue(!!errorDialog);

    // Test that the error dialog can be dismissed.
    const cancelButton =
        errorDialog.querySelector<CrButtonElement>('#cancelButton');
    assertTrue(!!cancelButton);
    cancelButton.click();
    await microtasksFinished();

    verifyInitialUIState(passwordsExporter);
  });

  // The error view allows to retry.
  test('exportFlowErrorRetry', async function() {
    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    await updateExportStatus(passwordManager, ExportProgressStatus.kInProgress);
    await updateExportStatus(
        passwordManager, ExportProgressStatus.kFailedWrite, 'tmp');
    await microtasksFinished();
    // Test that the error dialog is shown.
    const errorDialog =
        passwordsExporter.shadowRoot.querySelector<CrDialogElement>(
            '#dialogError');
    assertTrue(!!errorDialog);

    passwordManager.resetResolver('exportPasswords');
    // Test that clicking retry will start a new export.
    const tryAgainButton =
        errorDialog.querySelector<CrButtonElement>('#tryAgainButton');
    assertTrue(!!tryAgainButton);
    tryAgainButton.click();
    await microtasksFinished();

    await passwordManager.whenCalled('exportPasswords');
  });

  test('exportFlowErrorInProgress', async function() {
    passwordManager.setExportPasswordsResult(ExportPasswordsResult.kInProgress);

    clickExportPasswordsButton(passwordsExporter);
    await passwordManager.whenCalled('exportPasswords');

    // Wait for the .then block to execute.
    await microtasksFinished();

    assertTrue(
        isVisible(passwordsExporter.shadowRoot.querySelector('.spinner')));
  });
});
