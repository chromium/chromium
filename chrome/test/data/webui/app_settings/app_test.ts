// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://app-settings/web_app_settings.js';

import {App, AppManagementPermissionItemElement, AppManagementSupportedLinksItemElement, AppManagementSupportedLinksOverlappingAppsDialogElement, AppManagementToggleRowElement, AppType, BrowserProxy, createTriStatePermission, getPermissionValueBool, InstallReason, InstallSource, OptionalBool, PermissionType, PermissionTypeIndex, RunOnOsLoginMode, TriState, WebAppSettingsAppElement, WindowMode} from 'chrome://app-settings/web_app_settings.js';
import {CrRadioButtonElement} from 'chrome://resources/cr_elements/cr_radio_button/cr_radio_button.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestAppManagementBrowserProxy} from './test_app_management_browser_proxy.js';

type AppConfig = Partial<App>;

suite('AppSettingsAppTest', () => {
  let appSettingsApp: WebAppSettingsAppElement;
  let app: App;
  let testProxy: TestAppManagementBrowserProxy;

  function createApp(id: string, optConfig?: AppConfig): App {
    const app: App = {
      id: id,
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

  function fakeHandler() {
    return testProxy.fakeHandler;
  }

  function getSupportedLinksElement(): AppManagementSupportedLinksItemElement|
      null {
    return appSettingsApp.shadowRoot!
        .querySelector<AppManagementSupportedLinksItemElement>(
            'app-management-supported-links-item');
  }

  async function reloadPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    appSettingsApp = document.createElement('web-app-settings-app');
    document.body.appendChild(appSettingsApp);
    await waitAfterNextRender(appSettingsApp);
  }

  setup(async () => {
    app = createApp('test');
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

  test('supported links change preferred -> browser', async () => {
    const appOptions = {
      type: AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    await fakeHandler().setApp(createApp('app1', appOptions));
    await fakeHandler().flushPipesForTesting();
    await reloadPage();

    let radioGroup =
        getSupportedLinksElement()!.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('preferred', radioGroup.selected);

    const browserRadioButton =
        getSupportedLinksElement()!.shadowRoot!
            .querySelector<CrRadioButtonElement>('#browserRadioButton');
    assertTrue(!!browserRadioButton);
    await browserRadioButton.click();
    await fakeHandler().whenCalled('setPreferredApp');
    await flushTasks();

    const selectedApp = await fakeHandler().getApp('app1');
    assertTrue(!!selectedApp.app);
    assertFalse(selectedApp.app.isPreferredApp);

    radioGroup =
        getSupportedLinksElement()!.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('browser', radioGroup.selected);
  });

  test('supported links change browser -> preferred', async () => {
    const appOptions = {
      type: AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    // Add PWA app, and make it the currently selected app.
    await fakeHandler().setApp(createApp('app1', appOptions));
    await fakeHandler().flushPipesForTesting();
    await reloadPage();

    let radioGroup =
        getSupportedLinksElement()!.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('browser', radioGroup.selected);

    const preferredRadioButton =
        getSupportedLinksElement()!.shadowRoot!
            .querySelector<CrRadioButtonElement>('#preferredRadioButton');
    assertTrue(!!preferredRadioButton);
    await preferredRadioButton.click();
    await fakeHandler().whenCalled('setPreferredApp');
    await flushTasks();

    const selectedApp = await fakeHandler().getApp('app1');
    assertTrue(!!selectedApp.app);
    assertTrue(selectedApp.app.isPreferredApp);

    radioGroup =
        getSupportedLinksElement()!.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('preferred', radioGroup.selected);
  });

  test('overlap dialog is shown and accepted', async () => {
    const appOptions = {
      type: AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Add PWA app, and make it the currently selected app.
    await fakeHandler().setApp(createApp('app1', appOptions));
    await fakeHandler().addApp(createApp('app2', appOptions));
    fakeHandler().setOverlappingAppsForTesting(['app2']);
    await fakeHandler().flushPipesForTesting();
    await reloadPage();

    // Pre-test checks
    assertNull(getSupportedLinksElement()!.querySelector('#overlapDialog'));
    const browserRadioButton =
        getSupportedLinksElement()!.shadowRoot!
            .querySelector<CrRadioButtonElement>('#browserRadioButton');
    assertTrue(!!browserRadioButton);
    assertTrue(browserRadioButton.checked);

    // Open dialog
    let promise = fakeHandler().whenCalled('getOverlappingPreferredApps');
    const preferredRadioButton =
        getSupportedLinksElement()!.shadowRoot!
            .querySelector<CrRadioButtonElement>('#preferredRadioButton');
    assertTrue(!!preferredRadioButton);
    await preferredRadioButton.click();
    await promise;
    await fakeHandler().flushPipesForTesting();
    await flushTasks();
    assertTrue(!!getSupportedLinksElement()!.shadowRoot!.querySelector(
        '#overlapDialog'));

    // Accept change
    promise = fakeHandler().whenCalled('setPreferredApp');
    const overlapDialog = getSupportedLinksElement()!.shadowRoot!.querySelector<
        AppManagementSupportedLinksOverlappingAppsDialogElement>(
        '#overlapDialog');
    assertTrue(!!overlapDialog);
    overlapDialog.$.dialog.close();
    await promise;
    await fakeHandler().flushPipesForTesting();
    await flushTasks();

    assertNull(getSupportedLinksElement()!.shadowRoot!.querySelector(
        '#overlapDialog'));

    const selectedApp = await fakeHandler().getApp('app1');
    assertTrue(!!selectedApp.app);
    assertTrue(selectedApp.app.isPreferredApp);
    const radioGroup =
        getSupportedLinksElement()!.shadowRoot!.querySelector('cr-radio-group');
    assertTrue(!!radioGroup);
    assertEquals('preferred', radioGroup.selected);
  });

  test('overlap warning isnt shown when not selected', async () => {
    // Since pwaOptions1 is a preferred app, the overlap warning is not shown.
    const pwaOptions1 = {
      type: AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    const pwaOptions2 = {
      type: AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com'],
    };

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Add PWA app, and make it the currently selected app.
    await fakeHandler().setApp(createApp('app1', pwaOptions1));
    await fakeHandler().addApp(createApp('app2', pwaOptions2));
    fakeHandler().setOverlappingAppsForTesting(['app2']);
    await fakeHandler().flushPipesForTesting();
    await reloadPage();

    assertNull(getSupportedLinksElement()!.shadowRoot!.querySelector(
        '#overlapWarning'));
  });

  test('overlap warning is shown', async () => {
    // Since pwaOptions1 is not a preferred app, the overlap warning should be
    // shown.
    const pwaOptions1 = {
      type: AppType.kWeb,
      isPreferredApp: false,
      supportedLinks: ['google.com', 'gmail.com'],
    };

    const pwaOptions2 = {
      type: AppType.kWeb,
      isPreferredApp: true,
      supportedLinks: ['google.com'],
    };

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    // Add PWA app, and make it the currently selected app.
    await fakeHandler().setApp(createApp('app1', pwaOptions1));
    await fakeHandler().addApp(createApp('app2', pwaOptions2));
    fakeHandler().setOverlappingAppsForTesting(['app2']);
    await fakeHandler().flushPipesForTesting();
    await reloadPage();

    assertTrue(!!getSupportedLinksElement()!.shadowRoot!.querySelector(
        '#overlapWarning'));
  });
});
