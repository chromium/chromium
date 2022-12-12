// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SyncConfirmationBrowserProxy} from 'chrome://sync-confirmation/sync_confirmation_browser_proxy.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestSyncConfirmationBrowserProxy extends TestBrowserProxy
    implements SyncConfirmationBrowserProxy {
  constructor() {
    super([
      'confirm',
      'undo',
      'goToSettings',
      'initializedWithSize',
      'requestAccountInfo',
    ]);
  }

  confirm(description: string[], confirmation: string) {
    this.methodCalled('confirm', [description, confirmation]);
  }

  undo() {
    this.methodCalled('undo');
  }

  goToSettings(description: string[], confirmation: string) {
    this.methodCalled('goToSettings', [description, confirmation]);
  }

  initializedWithSize(height: number[]) {
    this.methodCalled('initializedWithSize', height);
  }

  requestAccountInfo() {
    this.methodCalled('requestAccountInfo');
  }
}
