// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {App} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {AppType, browserProxyFactory, InstallReason, InstallSource, PageHandlerRemote, PermissionType, RunOnOsLoginMode, TriState, WindowMode} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import type {MetricsBrowserProxy} from 'chrome://resources/cr_components/app_management/metrics_browser_proxy.js';
import {createTriStatePermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

export type AppConfig = Partial<App>;

export function setupMockHandler(): TestMock<PageHandlerRemote>&
    PageHandlerRemote {
  const handler = TestMock.fromClass(PageHandlerRemote);
  const {instance} = browserProxyFactory.createForTest(handler);
  browserProxyFactory.setInstance(instance);
  return handler;
}

export class TestMetricsBrowserProxy extends TestBrowserProxy implements
    MetricsBrowserProxy {
  constructor() {
    super(['recordEnumerationValue']);
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
      learnMoreUrl: 'https://google.com/',
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
    disableUserChoiceNavigationCapturing: false,
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
