// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter} from 'chrome://settings/lazy_load.js';
import type {BatchUploadPromoProxy, PageHandlerInterface, PageRemote} from 'chrome://settings/lazy_load.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBatchUploadPromoHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  // Settable fake data.
  private batchUploadPromoLocalDataCount_: number = 0;

  constructor() {
    super([
      'getBatchUploadPromoLocalDataCount',
      'onBatchUploadPromoClicked',
    ]);
  }

  setBatchUploadPromoLocalDataCount(localDataCount: number) {
    this.batchUploadPromoLocalDataCount_ = localDataCount;
  }

  getBatchUploadPromoLocalDataCount(): Promise<{localDataCount: number}> {
    return Promise.resolve(
        {localDataCount: this.batchUploadPromoLocalDataCount_});
  }

  onBatchUploadPromoClicked() {
    this.methodCalled('onBatchUploadPromoClicked');
  }
}

export class TestBatchUploadPromoProxy implements BatchUploadPromoProxy {
  callbackRouter: PageCallbackRouter;
  page: PageRemote;
  handler: TestBatchUploadPromoHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = new TestBatchUploadPromoHandler();
  }
}
