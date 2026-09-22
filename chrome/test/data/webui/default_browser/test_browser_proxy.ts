// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://default-browser-modal/browser_proxy.js';
import type {PageHandlerInterface, PageRemote} from 'chrome://default-browser-modal/default_browser_modal.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://default-browser-modal/default_browser_modal.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDefaultBrowserPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  isDefault: boolean = false;

  constructor() {
    super([
      'cancel',
      'confirm',
      'showUi',
      'tryAgain',
      'checkDefaultStatusAndMaybeClose',
    ]);
  }

  cancel() {
    this.methodCalled('cancel');
  }

  confirm() {
    this.methodCalled('confirm');
  }

  tryAgain() {
    this.methodCalled('tryAgain');
  }

  checkDefaultStatusAndMaybeClose() {
    this.methodCalled('checkDefaultStatusAndMaybeClose');
    return Promise.resolve({isDefault: this.isDefault});
  }

  // eslint-disable-next-line @typescript-eslint/naming-convention
  showUI() {
    this.methodCalled('showUi');
  }
}

export class TestDefaultBrowserBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: TestDefaultBrowserPageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = new TestDefaultBrowserPageHandler();
  }
}
