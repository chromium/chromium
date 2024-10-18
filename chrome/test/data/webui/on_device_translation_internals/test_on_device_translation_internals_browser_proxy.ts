// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface, PageRemote} from 'chrome://on-device-translation-internals/on_device_translation_internals.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://on-device-translation-internals/on_device_translation_internals.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestOnDeviceTranslationInternalsBrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: FakePageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();
    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
    this.handler = new FakePageHandler();
  }
}

export class FakePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  constructor() {
    super([
      'installLanguagePackage',
      'uninstallLanguagePackage',
    ]);
  }

  installLanguagePackage(packageIndex: number) {
    this.methodCalled('installLanguagePackage', packageIndex);
  }

  uninstallLanguagePackage(packageIndex: number) {
    this.methodCalled('uninstallLanguagePackage', packageIndex);
  }
}
