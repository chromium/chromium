// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://borealis-installer/error_dialog.js';

import {InstallResult} from 'chrome://borealis-installer/borealis_types.mojom-webui.js';
import type {BorealisInstallerErrorDialogElement} from 'chrome://borealis-installer/error_dialog.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<borealis-installer-error-dialog>', () => {
  let errorDialog: BorealisInstallerErrorDialogElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    errorDialog = document.createElement('borealis-installer-error-dialog');
    document.body.appendChild(errorDialog);
    await flushTasks();
  });

  teardown(function() {
    errorDialog.remove();
  });

  function shadowRoot(): ShadowRoot {
    const shadowRoot = errorDialog.shadowRoot;
    assertTrue(shadowRoot !== null);
    return shadowRoot;
  }

  const clickButton = async (id: string) => {
    const button = shadowRoot().getElementById(id);
    assertTrue(button != null);
    assertFalse(button.hidden);
    button.click();
    await flushTasks();
  };

  function assertHidden(id: string): void {
    const element = shadowRoot().getElementById(id);
    assertTrue(element != null);
    assertTrue(element.hidden);
  }

  function assertNotHidden(id: string): void {
    const element = shadowRoot().getElementById(id);
    assertTrue(element != null);
    assertFalse(element.hidden);
  }

  test('Buttons', () => {
    errorDialog.show(InstallResult.kDlcNeedSpaceError);

    let storageEventFired = false;
    errorDialog.addEventListener('storage', function() {
      storageEventFired = true;
    });

    clickButton('storage');
    assertTrue(storageEventFired);

    errorDialog.show(InstallResult.kDlcBusyError);

    let retryEventFired = false;
    errorDialog.addEventListener('retry', function() {
      retryEventFired = true;
    });

    clickButton('retry');
    assertTrue(retryEventFired);

    let cancelEventFired = false;
    errorDialog.addEventListener('cancel', function() {
      cancelEventFired = true;
    });

    clickButton('cancel');
    assertTrue(cancelEventFired);
  });


  test('UpdateError', () => {
    const results: number[] = [
      InstallResult.kBorealisNotAllowed,
      InstallResult.kDlcUnsupportedError,
      InstallResult.kDlcInternalError,
      InstallResult.kDlcUnknownError,
      InstallResult.kDlcNeedRebootError,
      InstallResult.kDlcNeedUpdateError,
    ];

    for (const result of results) {
      errorDialog.show(result);
      assertHidden('storage');
      assertHidden('retry');
      assertNotHidden('cancel');
      assertNotHidden('link');
    }
  });

  test('DuplicateError', () => {
    errorDialog.show(InstallResult.kBorealisInstallInProgress);
    assertHidden('storage');
    assertHidden('retry');
    assertNotHidden('cancel');
    assertHidden('link');
  });

  test('BusyError', () => {
    errorDialog.show(InstallResult.kDlcBusyError);
    assertHidden('storage');
    assertNotHidden('retry');
    assertNotHidden('cancel');
    assertHidden('link');
  });


  test('SpaceError', () => {
    errorDialog.show(InstallResult.kDlcNeedSpaceError);
    assertNotHidden('storage');
    assertHidden('retry');
    assertNotHidden('cancel');
    assertNotHidden('link');
  });


  test('OfflineError', () => {
    errorDialog.show(InstallResult.kOffline);

    assertHidden('storage');
    assertNotHidden('retry');
    assertNotHidden('cancel');
    assertNotHidden('link');
  });

  test('StartupError', () => {
    const results: number[] =
        [InstallResult.kStartupFailed, InstallResult.kMainAppNotPresent];

    for (const result of results) {
      errorDialog.show(result);
      assertHidden('storage');
      assertNotHidden('retry');
      assertNotHidden('cancel');
      assertHidden('link');
    }
  });
});
