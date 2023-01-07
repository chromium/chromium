// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App, PageCallbackRouter, PageHandlerInterface, PageRemote, Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {OptionalBool, RunOnOsLoginMode, WindowMode} from 'chrome://resources/cr_components/app_management/types.mojom-webui.js';

export class FakePageHandler implements PageHandlerInterface {
  private app_: App;
  private page_: PageRemote;
  private defaultAppAssociationsShown_: boolean;

  constructor(page: PageRemote, app: App) {
    this.page_ = page;
    this.app_ = app;
    this.defaultAppAssociationsShown_ = false;
  }

  setApp(app: App) {
    this.app_ = app;
    this.page_.onAppChanged(this.app_);
  }

  getApps() {
    return Promise.resolve({apps: []});
  }

  getApp(_appId: string) {
    return Promise.resolve({app: this.app_});
  }

  getExtensionAppPermissionMessages(_appId: string) {
    return Promise.resolve({messages: []});
  }

  setPinned(_appId: string, _pinned: OptionalBool) {}

  setPermission(_appId: string, permission: Permission) {
    this.app_.permissions[permission.permissionType] = permission;
    this.page_.onAppChanged(this.app_);
  }

  setResizeLocked(_appId: string, _locked: boolean) {}

  uninstall(_appId: string) {}

  openNativeSettings(_appId: string) {}

  setPreferredApp(_appId: string, _isPreferredApp: boolean) {}

  getOverlappingPreferredApps(_appId: string) {
    return Promise.resolve({appIds: []});
  }

  setWindowMode(_appId: string, windowMode: WindowMode) {
    this.app_.windowMode = windowMode;
    this.page_.onAppChanged(this.app_);
  }

  setRunOnOsLoginMode(_appId: string, loginMode: RunOnOsLoginMode) {
    this.app_.runOnOsLogin!.loginMode = loginMode;
    this.page_.onAppChanged(this.app_);
  }

  setFileHandlingEnabled(_appId: string, fileHandlingEnabled: boolean) {
    this.app_.fileHandlingState!.enabled = fileHandlingEnabled;
    this.page_.onAppChanged(this.app_);
  }

  showDefaultAppAssociationsUi() {
    this.defaultAppAssociationsShown_ = true;
  }
  defaultAppAssociationsUiWasShown() {
    return this.defaultAppAssociationsShown_;
  }

  openStorePage(_appId: string) {}
}

export class TestAppManagementBrowserProxy implements BrowserProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  handler: PageHandlerInterface;
  fakeHandler: FakePageHandler;

  constructor(app: App) {
    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();

    this.fakeHandler = new FakePageHandler(this.callbackRouterRemote, app);
    this.handler = this.fakeHandler;
  }

  recordEnumerationValue(metricName: string, value: number, enumSize: number) {
    chrome.metricsPrivate.recordEnumerationValue(metricName, value, enumSize);
  }
}
