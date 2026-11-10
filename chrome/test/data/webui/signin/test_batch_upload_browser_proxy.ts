// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from 'chrome://batch-upload/batch_upload.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBatchUploadHandler extends TestBrowserProxy implements
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
