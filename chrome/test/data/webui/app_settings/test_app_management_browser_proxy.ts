// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App, PageHandlerInterface, PageRemote, Permission, RunOnOsLoginMode, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {assert} from 'chrome://resources/js/assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class FakePageHandler extends TestBrowserProxy implements
    PageHandlerInterface {
  private app_: App;
  private page_: PageRemote|null = null;
  private overlappingApps_: string[] = [];
  private apps_: App[] = [];
  private defaultAppAssociationsShown_: boolean = false;

  constructor(app: App) {
    super([
      'setPreferredApp',
      'getOverlappingPreferredApps',
    ]);

    this.app_ = app;
  }

  setPage(page: PageRemote) {
    this.page_ = page;
  }

  async flushPipesForTesting() {
    await this.page_!.$.flushForTesting();
  }

  setOverlappingAppsForTesting(ids: string[]) {
    this.overlappingApps_ = ids;
  }

  // This is used to set the app for which the App Settings page
  // is being loaded.
  setApp(app: App) {
    this.app_ = app;
    this.apps_.push(app);
    this.page_!.onAppChanged(this.app_);
  }

  // This is used to mimic the addition of more apps, which can
  // be taken into account for a few components, like the
  // supported links item.
  addApp(app: App) {
    this.apps_.push(app);
    this.page_!.onAppChanged(app);
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
    this.page_!.onAppChanged(this.app_);
  }

  setResizeLocked(_appId: string, _locked: boolean) {}

  uninstall(_appId: string) {}

  updateAppSize(_appId: string) {}

  openNativeSettings(_appId: string) {}

  setWindowMode(_appId: string, windowMode: WindowMode) {
    this.app_.windowMode = windowMode;
    this.page_!.onAppChanged(this.app_);
  }

  setAppLocale(_appId: string, _localeTag: string): void {}

  setRunOnOsLoginMode(_appId: string, loginMode: RunOnOsLoginMode) {
    this.app_.runOnOsLogin!.loginMode = loginMode;
    this.page_!.onAppChanged(this.app_);
  }

  setFileHandlingEnabled(_appId: string, fileHandlingEnabled: boolean) {
    this.app_.fileHandlingState!.enabled = fileHandlingEnabled;
    this.page_!.onAppChanged(this.app_);
  }

  setPreferredApp(appId: string, isPreferredApp: boolean): void {
    const app = this.apps_.find((app) => app.id === appId);
    assert(app);

    this.app_ = {...app, isPreferredApp};
    this.page_!.onAppChanged(this.app_);
    this.methodCalled('setPreferredApp');
  }

  getOverlappingPreferredApps(_appId: string): Promise<{appIds: string[]}> {
    this.methodCalled('getOverlappingPreferredApps');
    if (!this.overlappingApps_) {
      return Promise.resolve({appIds: []});
    }
    return Promise.resolve({appIds: this.overlappingApps_});
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
