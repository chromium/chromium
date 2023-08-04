// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://app-settings/web_app_settings.js';

import {App, AppManagementPermissionItemElement, AppManagementToggleRowElement, AppType, BrowserProxy, createTriStatePermission, getPermissionValueBool, InstallReason, InstallSource, OptionalBool, PermissionType, PermissionTypeIndex, RunOnOsLoginMode, TriState, WebAppSettingsAppElement, WindowMode} from 'chrome://app-settings/web_app_settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestAppManagementBrowserProxy} from './test_app_management_browser_proxy.js';

suite('AppSettingsAppTest', () => {
  let appSettingsApp: WebAppSettingsAppElement;
  let app: App;
  let testProxy: TestAppManagementBrowserProxy;

  setup(async () => {
    app = {
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
      appSize: '',
      dataSize: '',
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

    testProxy = new TestAppManagementBrowserProxy(app);
    BrowserProxy.setInstance(testProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    appSettingsApp = document.createElement('web-app-settings-app');
    document.body.appendChild(appSettingsApp);
    await waitAfterNextRender(appSettingsApp);
  });

  test('Elements are present', function() {
    assertEquals(
        appSettingsApp.shadowRoot!.querySelector('.cr-title-text')!.textContent,
        app.title);

    assertTrue(!!appSettingsApp.shadowRoot!.querySelector('#title-icon'));

    assertTrue(!!appSettingsApp.shadowRoot!.querySelector(
        'app-management-uninstall-button'));

    assertTrue(!!appSettingsApp.shadowRoot!.querySelector(
        'app-management-more-permissions-item'));
  });

  test('Toggle Run on OS Login', function() {
    const runOnOsLoginItem = appSettingsApp.shadowRoot!.querySelector(
        'app-management-run-on-os-login-item')!;
    assertTrue(!!runOnOsLoginItem);
    assertEquals(
        runOnOsLoginItem.app.runOnOsLogin!.loginMode, RunOnOsLoginMode.kNotRun);

    runOnOsLoginItem.click();
    assertEquals(
        runOnOsLoginItem.app.runOnOsLogin!.loginMode,
        RunOnOsLoginMode.kWindowed);

    runOnOsLoginItem.click();
    assertEquals(
        runOnOsLoginItem.app.runOnOsLogin!.loginMode, RunOnOsLoginMode.kNotRun);
  });

  // Serves as a basic test of the presence of the File Handling item. More
  // comprehensive tests are located in the cross platform app_management test.
  test('Toggle File Handling', function() {
    const fileHandlingItem = appSettingsApp.shadowRoot!.querySelector(
        'app-management-file-handling-item')!;
    assertTrue(!!fileHandlingItem);
    assertEquals(fileHandlingItem.app.fileHandlingState!.enabled, false);

    const toggleRow =
        fileHandlingItem.shadowRoot!
            .querySelector<AppManagementToggleRowElement>('#toggle-row')!;
    assertTrue(!!toggleRow);
    toggleRow.click();
    assertEquals(fileHandlingItem.app.fileHandlingState!.enabled, true);

    toggleRow.click();
    assertEquals(fileHandlingItem.app.fileHandlingState!.enabled, false);
  });

  test('Toggle window mode', function() {
    const windowModeItem =
        appSettingsApp.shadowRoot!.querySelector('app-management-window-mode-item')!;
    assertTrue(!!windowModeItem);
    assertEquals(windowModeItem.app.windowMode, WindowMode.kWindow);

    windowModeItem.click();
    assertEquals(windowModeItem.app.windowMode, WindowMode.kBrowser);
  });

  test('Toggle permissions', function() {
    const permsisionTypes: PermissionTypeIndex[] =
        ['kNotifications', 'kLocation', 'kCamera', 'kMicrophone'];
    for (const permissionType of permsisionTypes) {
      const permissionItem = appSettingsApp.shadowRoot!.querySelector<
          AppManagementPermissionItemElement>(
          `app-management-permission-item[permission-type=${permissionType}]`)!;
      assertTrue(!!permissionItem);
      assertFalse(getPermissionValueBool(permissionItem.app, permissionType));

      permissionItem.click();
      assertTrue(getPermissionValueBool(permissionItem.app, permissionType));

      permissionItem.click();
      assertFalse(getPermissionValueBool(permissionItem.app, permissionType));
    }
  });
});
