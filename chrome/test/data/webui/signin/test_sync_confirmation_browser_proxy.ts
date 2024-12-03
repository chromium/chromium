// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ScreenMode, SyncConfirmationBrowserProxy} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSyncConfirmationBrowserProxy extends TestBrowserProxy
    implements SyncConfirmationBrowserProxy {
  constructor() {
    super([
      'confirm',
      'undo',
      'goToSettings',
      'openDeviceSyncSettings',
      'initializedWithSize',
      'requestAccountInfo',
    ]);
  }

  confirm(description: string[], confirmation: string, screenMode: ScreenMode) {
    this.methodCalled('confirm', [description, confirmation, screenMode]);
  }

  undo(screenMode: ScreenMode) {
    this.methodCalled('undo', [screenMode]);
  }

  goToSettings(
      description: string[], confirmation: string, screenMode: ScreenMode) {
    this.methodCalled('goToSettings', [description, confirmation, screenMode]);
  }

  openDeviceSyncSettings() {
    this.methodCalled('openDeviceSyncSettings');
  }

  initializedWithSize(height: number[]) {
    this.methodCalled('initializedWithSize', height);
  }

  requestAccountInfo() {
    this.methodCalled('requestAccountInfo');
  }
}
