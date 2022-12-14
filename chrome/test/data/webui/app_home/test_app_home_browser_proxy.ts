// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {AppInfo, ClickEvent, PageCallbackRouter, PageHandlerInterface, PageRemote, RunOnOsLoginMode} from 'chrome://apps/app_home.mojom-webui.js';
import {BrowserProxy} from 'chrome://apps/browser_proxy.js';

interface AppList {
  appList: AppInfo[];
}

export class FakePageHandler implements PageHandlerInterface {
  private apps_: AppList;

  constructor(apps: AppList) {
    this.apps_ = apps;
  }

  getApps() {
    return Promise.resolve(this.apps_);
  }

  uninstallApp(_appId: string) {}

  showAppSettings(_appId: string) {}

  createAppShortcut(_appId: string) {
    return Promise.resolve();
  }

  launchApp(_appId: string, _source: number, _clickEvent: ClickEvent) {}

  setRunOnOsLoginMode(_appId: string, _runOnOsLoginMode: RunOnOsLoginMode) {}

  launchDeprecatedAppDialog() {}

  installAppLocally(_appId: string) {}
}

export class TestAppHomeBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: PageHandlerInterface;
  fakeHandler: FakePageHandler;

  constructor(app: AppList) {
    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.fakeHandler = new FakePageHandler(app);
    this.handler = this.fakeHandler;
  }

  registerAppEnableEvent(_callback: Function) {}
}
