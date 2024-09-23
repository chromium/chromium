// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {LifetimeBrowserProxy} from 'chrome://os-settings/os_settings.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestLifetimeBrowserProxy extends TestBrowserProxy implements
    LifetimeBrowserProxy {
  private showRelaunchConfirmationDialog: boolean;
  private confirmationDialogDescription: string|null;

  constructor() {
    super([
      'restart',
      'relaunch',
      'signOutAndRestart',
      'factoryReset',
      'shouldShowRelaunchConfirmationDialog',
      'getRelaunchConfirmationDialogDescription',
    ]);

    this.showRelaunchConfirmationDialog = true;
    this.confirmationDialogDescription = null;
  }

  setRelaunchConfirmationDialog(showConfirmationDialog: boolean): void {
    this.showRelaunchConfirmationDialog = showConfirmationDialog;
  }

  setConfirmationDialogDescription(dialogDescription: string|null): void {
    this.confirmationDialogDescription = dialogDescription;
  }

  restart(): void {
    this.methodCalled('restart');
  }

  relaunch(): void {
    this.methodCalled('relaunch');
  }

  signOutAndRestart(): void {
    this.methodCalled('signOutAndRestart');
  }

  factoryReset(requestTpmFirmwareUpdate: boolean): void {
    this.methodCalled('factoryReset', requestTpmFirmwareUpdate);
  }

  shouldShowRelaunchConfirmationDialog(alwaysShowDialog: boolean):
      Promise<boolean> {
    this.methodCalled('shouldShowRelaunchConfirmationDialog', alwaysShowDialog);
    return Promise.resolve(this.showRelaunchConfirmationDialog);
  }

  getRelaunchConfirmationDialogDescription(isVersionUpdate: boolean):
      Promise<string|null> {
    this.methodCalled(
        'getRelaunchConfirmationDialogDescription', isVersionUpdate);
    return Promise.resolve(this.confirmationDialogDescription);
  }
}
