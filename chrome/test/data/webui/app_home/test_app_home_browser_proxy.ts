// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AppInfo, PageCallbackRouter, PageHandlerInterface} from 'chrome://apps/app_home.mojom-webui.js';
import {BrowserProxy} from 'chrome://apps/browser_proxy.js';

interface AppList {
  appList: AppInfo[];
}

export class FakePageHandler implements PageHandlerInterface {
  private app_: AppList;

  constructor(app: AppList) {
    this.app_ = app;
  }

  getApps() {
    return Promise.resolve(this.app_);
  }

  uninstallApp(_appId: string) {}
}

export class TestAppHomeBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;
  fakeHandler: FakePageHandler;

  constructor(app: AppList) {
    this.callbackRouter = new PageCallbackRouter();
    this.fakeHandler = new FakePageHandler(app);
    this.handler = this.fakeHandler;
  }

  registerAppRemoveEvent(_callback: Function) {}
}
