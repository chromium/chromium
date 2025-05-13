// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BaseDialogPageHandlerInterface} from 'chrome://privacy-sandbox-base-dialog/base_dialog.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestBaseDialogBrowserProxy {
  handler: TestBaseDialogPageHandler;

  constructor() {
    this.handler = new TestBaseDialogPageHandler();
  }
}

export class TestBaseDialogPageHandler extends TestBrowserProxy implements
    BaseDialogPageHandlerInterface {
  constructor() {
    super([
      'resizeDialog',
      'showDialog',
      'closeDialog',
    ]);
  }

  resizeDialog(height: number) {
    this.methodCalled('resizeDialog', height);
  }

  showDialog() {
    this.methodCalled('showDialog');
  }

  closeDialog() {
    this.methodCalled('closeDialog');
  }
}
