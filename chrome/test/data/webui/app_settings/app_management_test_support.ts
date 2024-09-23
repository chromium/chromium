// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, InstallReason, InstallSource, PageCallbackRouter, PageHandlerRemote, PermissionType, RunOnOsLoginMode, TriState, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {BrowserProxy} from 'chrome://resources/cr_components/app_management/browser_proxy.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export type AppConfig = Partial<App>;

export class TestAppManagementBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: TestMock<PageHandlerRemote>&PageHandlerRemote;

  constructor() {
    super(['recordEnumerationValue']);
    this.handler = TestMock.fromClass(PageHandlerRemote);
    this.callbackRouter = new PageCallbackRouter();
  }

  recordEnumerationValue(metricName: string, value: number, enumSize: number) {
    this.methodCalled('recordEnumerationValue', metricName, value, enumSize);
  }
}

export function createTestApp(id: string, optConfig?: AppConfig): App {
  const app: App = {
    id: id,
    type: AppType.kWeb,
    title: 'App Title',
    description: '',
    version: '5.1',
    size: '9.0MB',
    isPinned: false,
    isPolicyPinned: false,
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
    appSize: null,
    dataSize: null,
    publisherId: '',
    formattedOrigin: '',
    scopeExtensions: [],
    supportedLocales: [],
    selectedLocale: null,
    showSystemNotificationsSettingsLink: false,
    allowUninstall: true,
  };

  if (optConfig) {
    Object.assign(app, optConfig);
  }

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
