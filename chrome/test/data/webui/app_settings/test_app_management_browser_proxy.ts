// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App, PageHandlerInterface, PageRemote, Permission, RunOnOsLoginMode, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PageCallbackRouter} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {assert} from 'chrome://resources/js/assert.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';

export class FakePageHandler implements PageHandlerInterface {
  private app_: App;
  private page_: PageRemote;
  private overlappingApps_: string[];
  private apps_: App[];
  private defaultAppAssociationsShown_: boolean;
  private resolverMap_: Map<string, PromiseResolver<void>>;

  constructor(page: PageRemote, app: App) {
    this.app_ = app;
    this.page_ = page;
    this.overlappingApps_ = [];
    this.apps_ = [];

    this.defaultAppAssociationsShown_ = false;
    this.resolverMap_ = new Map();
    this.resolverMap_.set('setPreferredApp', new PromiseResolver());
    this.resolverMap_.set('getOverlappingPreferredApps', new PromiseResolver());
  }

  private getResolver_(methodName: string): PromiseResolver<void> {
    const method = this.resolverMap_.get(methodName);
    assert(method, `Method '${methodName}' not found.`);
    return method;
  }

  methodCalled(methodName: string): void {
    this.getResolver_(methodName).resolve();
  }

  async whenCalled(methodName: string): Promise<void> {
    await this.getResolver_(methodName).promise;
    this.resolverMap_.set(methodName, new PromiseResolver());
  }

  async flushPipesForTesting() {
    await this.page_.$.flushForTesting();
  }

  setOverlappingAppsForTesting(ids: string[]) {
    this.overlappingApps_ = ids;
  }

  // This is used to set the app for which the App Settings page
  // is being loaded.
  setApp(app: App) {
    this.app_ = app;
    this.apps_.push(app);
    this.page_.onAppChanged(this.app_);
  }

  // This is used to mimic the addition of more apps, which can
  // be taken into account for a few components, like the
  // supported links item.
  addApp(app: App) {
    this.apps_.push(app);
    this.page_.onAppChanged(app);
  }

  getApps() {
    return Promise.resolve({apps: this.apps_});
  }

  getApp(_appId: string) {
    return Promise.resolve({app: this.app_});
  }

  getSubAppToParentMap() {
    return Promise.resolve({subAppToParentMap: {}});
  }

  getExtensionAppPermissionMessages(_appId: string) {
    return Promise.resolve({messages: []});
  }

  setPinned(_appId: string, _pinned: boolean) { }

  setPermission(_appId: string, permission: Permission) {
    this.app_.permissions[permission.permissionType] = permission;
    this.page_.onAppChanged(this.app_);
  }

  setResizeLocked(_appId: string, _locked: boolean) {}

  uninstall(_appId: string) {}

  updateAppSize(_appId: string) {}

  openNativeSettings(_appId: string) {}

  setWindowMode(_appId: string, windowMode: WindowMode) {
    this.app_.windowMode = windowMode;
    this.page_.onAppChanged(this.app_);
  }

  setAppLocale(_appId: string, _localeTag: string): void {}

  setRunOnOsLoginMode(_appId: string, loginMode: RunOnOsLoginMode) {
    this.app_.runOnOsLogin!.loginMode = loginMode;
    this.page_.onAppChanged(this.app_);
  }

  setFileHandlingEnabled(_appId: string, fileHandlingEnabled: boolean) {
    this.app_.fileHandlingState!.enabled = fileHandlingEnabled;
    this.page_.onAppChanged(this.app_);
  }

  setPreferredApp(appId: string, isPreferredApp: boolean): void {
    const app = this.apps_.find((app) => app.id === appId);
    assert(app);

    this.app_ = {...app, isPreferredApp};
    this.page_.onAppChanged(this.app_);
    this.methodCalled('setPreferredApp');
  }

  async getOverlappingPreferredApps(_appId: string):
      Promise<{appIds: string[]}> {
    this.methodCalled('getOverlappingPreferredApps');
    if (!this.overlappingApps_) {
      return {appIds: []};
    }
    return {appIds: this.overlappingApps_};
  }

  showDefaultAppAssociationsUi() {
    this.defaultAppAssociationsShown_ = true;
  }

  defaultAppAssociationsUiWasShown() {
    return this.defaultAppAssociationsShown_;
  }

  openStorePage(_appId: string) {}

  openSystemNotificationSettings(_appId: string) {}
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
