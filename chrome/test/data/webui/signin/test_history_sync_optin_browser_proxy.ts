// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from 'chrome://history-sync-optin/history_sync_optin.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestHistorySyncOptInHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'accept',
      'reject',
      'requestAccountInfo',
      'updateDialogHeight',
    ]);
  }

  accept() {
    this.methodCalled('accept');
  }

  reject() {
    this.methodCalled('reject');
  }

  requestAccountInfo() {
    this.methodCalled('requestAccountInfo');
  }

  updateDialogHeight(height: number) {
    this.methodCalled('updateDialogHeight', height);
  }
}
