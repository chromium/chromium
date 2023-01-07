// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {App, PageCallbackRouter, PageHandlerRemote, PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {AppType, InstallReason, InstallSource, OptionalBool, RunOnOsLoginMode, WindowMode} from 'chrome://resources/cr_components/app_management/types.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestAppManagementBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote&TestBrowserProxy;

  constructor() {
    super(['recordEnumerationValue']);
    this.handler = TestBrowserProxy.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
  }

  recordEnumerationValue(metricName: string, value: number, enumSize: number) {
    this.methodCalled('recordEnumerationValue', metricName, value, enumSize);
  }
}

export function createTestApp(): App {
  const app: App = {
    id: 'test_loader.html',
    type: AppType.kWeb,
    title: 'App Title',
    description: '',
    version: '5.1',
    size: '9.0MB',
    isPinned: OptionalBool.kFalse,
    isPolicyPinned: OptionalBool.kFalse,
    installReason: InstallReason.kUser,
    permissions: {},
    hideMoreSettings: false,
    hidePinToShelf: false,
    isPreferredApp: false,
    windowMode: WindowMode.kWindow,
    hideWindowMode: false,
    resizeLocked: false,
    hideResizeLocked: true,
    supportedLinks: [],
    runOnOsLogin: {loginMode: RunOnOsLoginMode.kNotRun, isManaged: false},
    fileHandlingState: {
      enabled: false,
      isManaged: false,
      userVisibleTypes: 'TXT',
      userVisibleTypesLabel: 'Supported type: TXT',
      learnMoreUrl: {url: 'https://google.com/'},
    },
    installSource: InstallSource.kUnknown,
    appSize: undefined,
    dataSize: undefined,
    publisherId: '',
  };

  const permissionTypes = [
    PermissionType.kLocation,
    PermissionType.kNotifications,
    PermissionType.kMicrophone,
    PermissionType.kCamera,
  ];

  for (const permissionType of permissionTypes) {
    const permissionValue = TriState.kAsk;
    const isManaged = false;
    app.permissions[permissionType] =
        createTriStatePermission(permissionType, permissionValue, isManaged);
  }
  return app;
}
