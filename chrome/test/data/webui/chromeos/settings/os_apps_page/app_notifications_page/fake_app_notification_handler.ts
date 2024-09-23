// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appNotificationHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {Permission, PermissionType} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

const {AppNotificationsObserverRemote} = appNotificationHandlerMojom;

type App = appNotificationHandlerMojom.App;
type AppNotificationsHandlerInterface =
    appNotificationHandlerMojom.AppNotificationsHandlerInterface;
type AppNotificationsObserverRemoteType =
    appNotificationHandlerMojom.AppNotificationsObserverRemote;

export class FakeAppNotificationHandler implements
    AppNotificationsHandlerInterface {
  private resolverMap_: Map<string, PromiseResolver<any>>;
  private appNotificationsObserverRemote_: AppNotificationsObserverRemoteType;
  private isQuietModeEnabled_: boolean;
  private lastUpdatedAppId_: string;
  private lastUpdatedAppPermission_: Permission;
  private apps_: App[];
  constructor() {
    this.resolverMap_ = new Map();
    this.appNotificationsObserverRemote_ = new AppNotificationsObserverRemote();
    this.isQuietModeEnabled_ = false;
    this.lastUpdatedAppId_ = '-1';
    this.lastUpdatedAppPermission_ = {
      permissionType: PermissionType.kUnknown,
      isManaged: false,
      value: {},
      details: null,
    };
    this.apps_ = [];

    this.resetForTest();
  }

  resetForTest(): void {
    if (this.appNotificationsObserverRemote_) {
      this.appNotificationsObserverRemote_ =
          new AppNotificationsObserverRemote();
    }

    this.apps_ = [];
    this.isQuietModeEnabled_ = false;
    this.lastUpdatedAppId_ = '-1';
    this.lastUpdatedAppPermission_ = {
      permissionType: PermissionType.kUnknown,
      isManaged: false,
      value: {},
      details: null,
    };

    this.resolverMap_.set('addObserver', new PromiseResolver());
    this.resolverMap_.set('getQuietMode', new PromiseResolver());
    this.resolverMap_.set('setQuietMode', new PromiseResolver());
    this.resolverMap_.set('setNotificationPermission', new PromiseResolver());
    this.resolverMap_.set('getApps', new PromiseResolver());
  }

  private getResolver_(methodName: string): PromiseResolver<void> {
    const method = this.resolverMap_.get(methodName);
    assertTrue(!!method, `Method '${methodName}' not found.`);
    return method;
  }

  protected methodCalled(methodName: string): void {
    this.getResolver_(methodName).resolve();
  }

  async whenCalled(methodName: string): Promise<void> {
    await this.getResolver_(methodName).promise;
    // Support sequential calls to whenCalled by replacing the promise.
    this.resolverMap_.set(methodName, new PromiseResolver());
  }

  getObserverRemote(): AppNotificationsObserverRemoteType {
    return this.appNotificationsObserverRemote_;
  }

  getCurrentQuietModeState(): boolean {
    return this.isQuietModeEnabled_;
  }

  getLastUpdatedAppId(): string {
    return this.lastUpdatedAppId_;
  }

  getLastUpdatedPermission(): Permission {
    return this.lastUpdatedAppPermission_;
  }

  // appNotificationHandler methods

  addObserver(remote: AppNotificationsObserverRemoteType): Promise<void> {
    this.appNotificationsObserverRemote_ = remote;
    this.methodCalled('addObserver');
    return Promise.resolve();
  }

  getQuietMode(): Promise<{enabled: boolean}> {
    this.methodCalled('getQuietMode');
    return Promise.resolve({enabled: this.isQuietModeEnabled_});
  }

  setQuietMode(enabled: boolean): Promise<{success: boolean}> {
    this.isQuietModeEnabled_ = enabled;
    this.methodCalled('setQuietMode');
    return Promise.resolve({success: true});
  }

  openBrowserNotificationSettings(): void {
    this.methodCalled('openBrowserNotificationSettings');
  }

  setNotificationPermission(id: string, permission: Permission):
      Promise<{success: boolean}> {
    this.lastUpdatedAppId_ = id;
    this.lastUpdatedAppPermission_ = permission;
    this.methodCalled('setNotificationPermission');
    return Promise.resolve({success: true});
  }

  getApps(): Promise<{apps: App[]}> {
    this.methodCalled('getApps');
    return Promise.resolve({apps: this.apps_});
  }
}
