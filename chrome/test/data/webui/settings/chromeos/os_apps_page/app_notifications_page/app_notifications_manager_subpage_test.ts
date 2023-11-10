// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsAppNotificationsManagerSubpage} from 'chrome://os-settings/lazy_load.js';
import {appNotificationHandlerMojom, createRouterForTesting, CrToggleElement, Router, routes, setAppNotificationProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermission, getBoolPermissionValue, isBoolValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppNotificationHandler} from './fake_app_notification_handler.js';

const {Readiness} = appNotificationHandlerMojom;

type App = appNotificationHandlerMojom.App;
type ReadinessType = appNotificationHandlerMojom.Readiness;

suite('<settings-app-notifications-manager-subpage>', () => {
  let page: SettingsAppNotificationsManagerSubpage;
  let mojoApi: FakeAppNotificationHandler;

  function createPage(): void {
    Router.getInstance().navigateTo(routes.APP_NOTIFICATIONS_MANAGER);
    page = document.createElement('settings-app-notifications-manager-subpage');
    document.body.appendChild(page);
    flush();
  }

  suiteSetup(() => {
    mojoApi = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi);
  });

  setup(() => {
    // Reinitialize Router and routes based on load time data
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);
  });

  teardown(() => {
    mojoApi.resetForTest();
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function initializeObserver(): Promise<void> {
    return mojoApi.whenCalled('addObserver');
  }

  function simulateNotificationAppChanged(app: App): void {
    mojoApi.getObserverRemote().onNotificationAppChanged(app);
  }

  function createApp(
      id: string, title: string, permission: Permission,
      readiness: ReadinessType = Readiness.kReady): App {
    return {
      id,
      title,
      notificationPermission: permission,
      readiness,
    };
  }

  test(
      'Manage app notifications list shows the correct number of apps.',
      async () => {
        createPage();
        const appTitle1 = 'Files';
        const appTitle2 = 'Chrome';
        const permission1 = createBoolPermission(
            /**permissionType=*/ 1,
            /**value=*/ false, /**is_managed=*/ false);
        const permission2 = createBoolPermission(
            /**permissionType=*/ 2,
            /**value=*/ true, /**is_managed=*/ false);
        const app1 = createApp('file-id', appTitle1, permission1);
        const app2 = createApp('chrome-id', appTitle2, permission2);

        await initializeObserver();
        simulateNotificationAppChanged(app1);
        simulateNotificationAppChanged(app2);
        await flushTasks();

        const appNotificationsList =
            page.shadowRoot!.querySelector('#appNotificationsList');
        assertTrue(isVisible(appNotificationsList));

        const appsList =
            appNotificationsList!.querySelectorAll('app-notification-row');
        assertEquals(2, appsList.length);
      });

  test('load the app list and toggle the notification permission', async () => {
    createPage();
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ false);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('1', 'App1', permission1);
    const app2 = createApp('2', 'App2', permission2);

    await initializeObserver();
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    // Expect 2 apps to be loaded.
    const appNotificationsList =
        page.shadowRoot!.querySelector('#appNotificationsList');
    assertTrue(!!appNotificationsList);
    const appRowList =
        appNotificationsList.querySelectorAll('app-notification-row');
    assertEquals(2, appRowList.length);

    const appRow1 = appRowList[0];
    assertTrue(!!appRow1);
    let appToggle =
        appRow1.shadowRoot!.querySelector<CrToggleElement>('#appToggle');
    assertTrue(!!appToggle);
    assertFalse(appToggle.checked);
    assertFalse(appToggle.disabled);
    let appTitle = appRow1.shadowRoot!.querySelector('#appTitle');
    assertTrue(!!appTitle);
    assertEquals('App1', appTitle.textContent?.trim());

    const appRow2 = appRowList[1];
    assertTrue(!!appRow2);
    appToggle =
        appRow2.shadowRoot!.querySelector<CrToggleElement>('#appToggle');
    assertTrue(!!appToggle);
    assertTrue(appToggle.checked);
    assertFalse(appToggle.disabled);
    appTitle = appRow2.shadowRoot!.querySelector('#appTitle');
    assertTrue(!!appTitle);
    assertEquals('App2', appTitle.textContent?.trim());

    // Click on the first app's toggle.
    appToggle =
        appRow1.shadowRoot!.querySelector<CrToggleElement>('#appToggle');
    assertTrue(!!appToggle);
    appToggle.click();

    await mojoApi.whenCalled('setNotificationPermission');

    // Verify that the sent message matches the app it was clicked from.
    assertEquals('1', mojoApi.getLastUpdatedAppId());
    const lastUpdatedPermission = mojoApi.getLastUpdatedPermission();
    assertEquals(1, lastUpdatedPermission.permissionType);
    assertTrue(isBoolValue(lastUpdatedPermission.value));
    assertFalse(lastUpdatedPermission.isManaged);
    assertTrue(getBoolPermissionValue(lastUpdatedPermission.value));
  });

  test(
      'the correct app disappears from the notification page when the user uninstalls it',
      async () => {
        createPage();
        const permission1 = createBoolPermission(
            /**permissionType=*/ 1,
            /**value=*/ false, /**is_managed=*/ false);
        const permission2 = createBoolPermission(
            /**permissionType=*/ 2,
            /**value=*/ true, /**is_managed=*/ false);
        const app1 = createApp('1', 'App1', permission1);
        const app2 = createApp('2', 'App2', permission2);

        await initializeObserver();
        simulateNotificationAppChanged(app1);
        simulateNotificationAppChanged(app2);
        await flushTasks();

        // Expect 2 apps to be loaded.
        const appNotificationsList =
            page.shadowRoot!.querySelector('#appNotificationsList');
        assertTrue(!!appNotificationsList);
        let appRowList =
            appNotificationsList.querySelectorAll('app-notification-row');
        assertEquals(2, appRowList.length);

        const app3 =
            createApp('1', 'App1', permission1, Readiness.kUninstalledByUser);
        simulateNotificationAppChanged(app3);

        await flushTasks();
        // Expect only 1 app to be shown now.
        appRowList =
            appNotificationsList.querySelectorAll('app-notification-row');
        assertEquals(1, appRowList.length);

        const appRow1 = appRowList[0];
        assertTrue(!!appRow1);
        const appTitle = appRow1.shadowRoot!.querySelector('#appTitle');
        assertTrue(!!appTitle);
        assertEquals('App2', appTitle.textContent?.trim());
      });

  test('Each app-notification-row displays correctly', async () => {
    createPage();
    const appTitle1 = 'Files';
    const appTitle2 = 'Chrome';
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ true);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('file-id', appTitle1, permission1);
    const app2 = createApp('chrome-id', appTitle2, permission2);

    await initializeObserver();
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    await flushTasks();

    const appNotificationsList =
        page.shadowRoot!.querySelector('#appNotificationsList');
    assertTrue(!!appNotificationsList);
    const chromeRow = appNotificationsList.children[0];
    const filesRow = appNotificationsList.children[1];

    assertTrue(!!page);
    assertTrue(!!chromeRow);
    assertTrue(!!filesRow);
    flush();

    // Apps should be listed in alphabetical order. |appTitle1| should come
    // before |appTitle2|, so a 1 should be returned by localCompare.
    const expected = 1;
    const actual = appTitle1.localeCompare(appTitle2);
    assertEquals(expected, actual);

    let appTitle = chromeRow.shadowRoot!.querySelector('#appTitle');
    assertTrue(!!appTitle);
    let appToggle =
        chromeRow.shadowRoot!.querySelector<CrToggleElement>('#appToggle');
    assertTrue(!!appToggle);
    assertEquals(appTitle2, appTitle.textContent?.trim());
    assertFalse(appToggle.disabled);
    assertNull(chromeRow.shadowRoot!.querySelector('cr-policy-indicator'));

    appTitle = filesRow.shadowRoot!.querySelector('#appTitle');
    assertTrue(!!appTitle);
    appToggle =
        filesRow.shadowRoot!.querySelector<CrToggleElement>('#appToggle');
    assertTrue(!!appToggle);
    assertEquals(appTitle1, appTitle.textContent?.trim());
    assertTrue(appToggle.disabled);
    assertTrue(!!filesRow.shadowRoot!.querySelector('cr-policy-indicator'));
  });

  test('App list filters when searching', async () => {
    createPage();
    const appTitle1 = 'Files';
    const appTitle2 = 'Google Chat';
    const appTitle3 = 'Google Calendar';
    const permission1 = createBoolPermission(
        /**permissionType=*/ 1,
        /**value=*/ false, /**is_managed=*/ false);
    const permission2 = createBoolPermission(
        /**permissionType=*/ 2,
        /**value=*/ true, /**is_managed=*/ false);
    const app1 = createApp('file', appTitle1, permission1);
    const app2 = createApp('chat', appTitle2, permission2);
    const app3 = createApp('calendar', appTitle3, permission1);

    await initializeObserver();
    simulateNotificationAppChanged(app1);
    simulateNotificationAppChanged(app2);
    simulateNotificationAppChanged(app3);
    await flushTasks();

    const appNotificationsList =
        page.shadowRoot!.querySelector('#appNotificationsList');
    assertTrue(isVisible(appNotificationsList));

    let appsList =
        appNotificationsList!.querySelectorAll('app-notification-row');
    assertEquals(3, appsList.length);

    page.searchTerm = 'Google';
    await flushTasks();
    appsList = appNotificationsList!.querySelectorAll('app-notification-row');
    assertEquals(2, appsList.length);

    // Uninstall the calender app. Now the app list should only have chat app
    // with search term "Google";
    app3.readiness = Readiness.kUninstalledByUser;
    simulateNotificationAppChanged(app3);
    await flushTasks();
    appsList = appNotificationsList!.querySelectorAll('app-notification-row');
    assertEquals(1, appsList.length);

    page.searchTerm = 'sl';
    await flushTasks();
    appsList = appNotificationsList!.querySelectorAll('app-notification-row');
    assertEquals(0, appsList.length);
    // "No apps found" label shows when no apps could be found with search term
    // "sl";
    const noAppsLabel =
        page.shadowRoot!.querySelector<HTMLElement>('#noAppsLabel');
    assertTrue(!!noAppsLabel);
    assertTrue(isVisible(noAppsLabel));

    page.searchTerm = '';
    await flushTasks();
    appsList = appNotificationsList!.querySelectorAll('app-notification-row');
    assertEquals(2, appsList.length);
  });
});
