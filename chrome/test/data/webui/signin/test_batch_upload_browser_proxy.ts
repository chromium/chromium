// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter} from 'chrome://batch-upload/batch_upload.js';
import type {BatchUploadBrowserProxy, PageHandlerInterface} from 'chrome://batch-upload/batch_upload.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestBatchUploadHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'updateViewHeight',
      'close',
      'saveToAccount',
    ]);
  }

  updateViewHeight(height: number) {
    this.methodCalled('updateViewHeight', height);
  }

  close() {
    this.methodCalled('close');
  }

  saveToAccount(idsToMove: number[][]) {
    this.methodCalled('saveToAccount', idsToMove);
  }
}

export class TestBatchUploadBrowserProxy implements BatchUploadBrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: TestBatchUploadHandler = new TestBatchUploadHandler();
}
