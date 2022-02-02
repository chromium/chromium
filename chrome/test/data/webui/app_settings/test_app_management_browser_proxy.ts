// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App, PageCallbackRouter, PageHandlerInterface, PageRemote} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {OptionalBool, Permission, WindowMode} from 'chrome://resources/cr_components/app_management/types.mojom-webui.js';

export class FakePageHandler implements PageHandlerInterface {
  private app: App|null = null;

  setApp(app: App) {
    this.app = app;
  }

  getApps() {
    return Promise.resolve({apps: []});
  }

  getApp(_appId: string) {
    return Promise.resolve({app: this.app});
  }

  getExtensionAppPermissionMessages(_appId: string) {
    return Promise.resolve({messages: []});
  }

  setPinned(_appId: string, _pinned: OptionalBool) {}

  setPermission(_appId: string, _permission: Permission) {}

  setResizeLocked(_appId: string, _locked: boolean) {}

  uninstall(_appId: string) {}

  openNativeSettings(_appId: string) {}

  setPreferredApp(_appId: string, _isPreferredApp: boolean) {}

  getOverlappingPreferredApps(_appId: string) {
    return Promise.resolve({appIds: []});
  }

  setWindowMode(_appId: string, _windowMode: WindowMode) {}
}

export class TestAppManagementBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: PageHandlerInterface;
  fakeHandler: FakePageHandler;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.fakeHandler = new FakePageHandler();
    this.handler = this.fakeHandler;
  }
}
