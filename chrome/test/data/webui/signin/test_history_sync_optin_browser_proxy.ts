// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {HistorySyncOptInBrowserProxy} from 'chrome://history-sync-optin/browser_proxy.js';
import type {PageRemote} from 'chrome://history-sync-optin/history_sync_optin.mojom-webui.js';
import {PageCallbackRouter, type PageHandlerInterface} from 'chrome://history-sync-optin/history_sync_optin.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestHistorySyncOptInHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  callbackRouter: PageCallbackRouter;
  constructor() {
    super([
      'accept',
      'reject',
      'requestAccountInfo',
      'updateDialogHeight',
    ]);
    this.callbackRouter = new PageCallbackRouter();
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

export class TestHistorySyncOptInBrowserProxy extends TestBrowserProxy
    implements HistorySyncOptInBrowserProxy {
  callbackRouter: PageCallbackRouter;
  page: PageRemote;
  handler: TestHistorySyncOptInHandler;

  constructor() {
    super();
    this.callbackRouter = new PageCallbackRouter();
    this.page =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = new TestHistorySyncOptInHandler();
  }
}
