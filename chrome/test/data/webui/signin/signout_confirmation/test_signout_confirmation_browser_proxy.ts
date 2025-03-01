// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter} from 'chrome://signout-confirmation/signout_confirmation.js';
import type {PageHandlerInterface, SignoutConfirmationBrowserProxy} from 'chrome://signout-confirmation/signout_confirmation.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

class TestSignoutConfirmationHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'updateViewHeight',
      'accept',
      'cancel',
      'close',
    ]);
  }

  updateViewHeight(height: number) {
    this.methodCalled('updateViewHeight', height);
  }

  accept(uninstallAccountExtensions: boolean) {
    this.methodCalled('accept', uninstallAccountExtensions);
  }

  cancel(uninstallAccountExtensions: boolean) {
    this.methodCalled('cancel', uninstallAccountExtensions);
  }

  close() {
    this.methodCalled('close');
  }
}

export class TestSignoutConfirmationBrowserProxy implements
    SignoutConfirmationBrowserProxy {
  callbackRouter: PageCallbackRouter = new PageCallbackRouter();
  handler: TestSignoutConfirmationHandler =
      new TestSignoutConfirmationHandler();
}
