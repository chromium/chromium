// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {appPermissionHandlerMojom} from 'chrome://os-settings/os_settings.js';
import {Permission, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';

import {createApp} from './privacy_hub_app_permission_test_util.js';

const {AppPermissionsObserverRemote} = appPermissionHandlerMojom;

type App = appPermissionHandlerMojom.App;
type AppPermissionsHandlerInterface =
    appPermissionHandlerMojom.AppPermissionsHandlerInterface;
type AppPermissionsObserverRemoteType =
    appPermissionHandlerMojom.AppPermissionsObserverRemote;

export class FakeAppPermissionHandler implements
    AppPermissionsHandlerInterface {
  private resolverMap_: Map<string, PromiseResolver<any>>;
  private appPermissionsObserverRemote_: AppPermissionsObserverRemoteType;
  private lastOpenedBrowserPermissionSettingsType_: PermissionType;
  private lastUpdatedAppPermission_: Permission;
  private nativeSettingsOpenedCount_: number;

  constructor() {
    this.resolverMap_ = new Map();
    this.resolverMap_.set('addObserver', new PromiseResolver());
    this.resolverMap_.set('getApps', new PromiseResolver());
    this.resolverMap_.set('getSystemAppsThatUseCamera', new PromiseResolver());
    this.resolverMap_.set(
        'getSystemAppsThatUseMicrophone', new PromiseResolver());
    this.resolverMap_.set(
        'openBrowserPermissionSettings', new PromiseResolver());
    this.resolverMap_.set('openNativeSettings', new PromiseResolver());
    this.resolverMap_.set('setPermission', new PromiseResolver());
    this.appPermissionsObserverRemote_ = new AppPermissionsObserverRemote();
    this.lastUpdatedAppPermission_ = {
      permissionType: PermissionType.kUnknown,
      isManaged: false,
      value: {},
      details: null,
    };
    this.lastOpenedBrowserPermissionSettingsType_ = PermissionType.kUnknown;
    this.nativeSettingsOpenedCount_ = 0;
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

  getObserverRemote(): AppPermissionsObserverRemoteType {
    return this.appPermissionsObserverRemote_;
  }

  getLastOpenedBrowserPermissionSettingsType(): PermissionType {
    return this.lastOpenedBrowserPermissionSettingsType_;
  }

  getLastUpdatedPermission(): Permission {
    return this.lastUpdatedAppPermission_;
  }

  getNativeSettingsOpenedCount(): number {
    return this.nativeSettingsOpenedCount_;
  }

  // appPermissionHandler methods.
  addObserver(remote: AppPermissionsObserverRemoteType): Promise<void> {
    this.appPermissionsObserverRemote_ = remote;
    this.methodCalled('addObserver');
    return Promise.resolve();
  }

  getApps(): Promise<{apps: App[]}> {
    this.methodCalled('getApps');
    return Promise.resolve({apps: []});
  }

  getSystemAppsThatUseCamera(): Promise<{apps: App[]}> {
    this.methodCalled('getSystemAppsThatUseCamera');
    return Promise.resolve({
      apps: [createApp(
          'app1_id', 'app1_name', PermissionType.kCamera, TriState.kAllow)],
    });
  }

  getSystemAppsThatUseMicrophone(): Promise<{apps: App[]}> {
    this.methodCalled('getSystemAppsThatUseMicrophone');
    return Promise.resolve({
      apps: [createApp(
          'app1_id', 'app1_name', PermissionType.kMicrophone, TriState.kAllow)],
    });
  }

  setPermission(id: string, permission: Permission):
      Promise<{success: boolean}> {
    assertTrue(!!id);
    this.lastUpdatedAppPermission_ = permission;
    this.methodCalled('setPermission');
    return Promise.resolve({success: true});
  }

  openNativeSettings(id: string): Promise<{success: boolean}> {
    assertTrue(!!id);
    this.nativeSettingsOpenedCount_++;
    this.methodCalled('openNativeSettings');
    return Promise.resolve({success: true});
  }

  openBrowserPermissionSettings(permissionType: PermissionType):
      Promise<{success: boolean}> {
    this.lastOpenedBrowserPermissionSettingsType_ = permissionType;
    this.methodCalled('openBrowserPermissionSettings');
    return Promise.resolve({success: true});
  }
}
