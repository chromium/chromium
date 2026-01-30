// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BrowserProxy} from 'chrome://default-browser-modal/browser_proxy.js';
import type {PageHandlerInterface} from 'chrome://default-browser-modal/default_browser_modal.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://default-browser-modal/default_browser_modal.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestDefaultBrowserPageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'cancel',
      'confirm',
    ]);
  }

  cancel() {
    this.methodCalled('cancel');
  }

  confirm() {
    this.methodCalled('confirm');
  }
}

export class TestDefaultBrowserBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestDefaultBrowserPageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.handler = new TestDefaultBrowserPageHandler();
  }
}
