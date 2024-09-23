// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AppNotificationsSubpage} from 'chrome://os-settings/lazy_load.js';
import {appNotificationHandlerMojom, CrToggleElement, OsSettingsRoutes, Router, routes, setAppNotificationProviderForTesting, SettingsToggleButtonElement, setUserActionRecorderForTesting} from 'chrome://os-settings/os_settings.js';
import {Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermission, getBoolPermissionValue, isBoolValue} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeUserActionRecorder} from '../../fake_user_action_recorder.js';

import {FakeAppNotificationHandler} from './fake_app_notification_handler.js';

const {Readiness} = appNotificationHandlerMojom;

type App = appNotificationHandlerMojom.App;
type ReadinessType = appNotificationHandlerMojom.Readiness;

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<settings-app-notifications-subpage>', () => {
  let page: AppNotificationsSubpage;
  let mojoApi: FakeAppNotificationHandler;
  let setQuietModeCounter = 0;
  let userActionRecorder: FakeUserActionRecorder;
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');

  function createPage(): void {
    page = document.createElement('settings-app-notifications-subpage');
    document.body.appendChild(page);
    assertTrue(!!page);
    flush();
  }

  suiteSetup(() => {
    mojoApi = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi);
  });

  setup(() => {
    userActionRecorder = new FakeUserActionRecorder();
    setUserActionRecorderForTesting(userActionRecorder);
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
  });

  teardown(() => {
    mojoApi.resetForTest();
    page.remove();
  });

  function initializeObserver(): Promise<void> {
    return mojoApi.whenCalled('addObserver');
  }

  function simulateQuickSettings(enable: boolean): void {
    mojoApi.getObserverRemote().onQuietModeChanged(enable);
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

  if (isRevampWayfindingEnabled) {
    const subpageTriggerData: SubpageTriggerData[] = [
      {
        triggerSelector: '#appNotificationsManagerRow',
        routeName: 'APP_NOTIFICATIONS_MANAGER',
      },
    ];
    subpageTriggerData.forEach(({triggerSelector, routeName}) => {
      test(
          `${routeName} subpage trigger is focused when returning from subpage`,
          async () => {
            createPage();

            const subpageTrigger =
                page.shadowRoot!.querySelector<HTMLButtonElement>(
                    triggerSelector);
            assertTrue(!!subpageTrigger);

            // Sub-page trigger navigates to Detailed build info subpage
            subpageTrigger.click();
            assertEquals(routes[routeName], Router.getInstance().currentRoute);

            // Navigate back
            const popStateEventPromise = eventToPromise('popstate', window);
            Router.getInstance().navigateToPreviousRoute();
            await popStateEventPromise;
            await waitAfterNextRender(page);

            assertEquals(
                subpageTrigger, page.shadowRoot!.activeElement,
                `${triggerSelector} should be focused.`);
          });
    });

    test('Manage app notifications row appears.', () => {
      createPage();

      const row = page.shadowRoot!.querySelector('#appNotificationsManagerRow');
      assertTrue(isVisible(row));
    });
  } else {
    test('Manage app notifications row does not appear.', () => {
      createPage();

      const row = page.shadowRoot!.querySelector('#appNotificationsManagerRow');
      assertFalse(isVisible(row));
    });

    test('loadAppListAndClickToggle', async () => {
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

    test('RemovedApp', async () => {
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
  }

  test('toggleDoNotDisturb', () => {
    createPage();
    const dndToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#doNotDisturbToggle');
    assertTrue(!!dndToggle);

    flush();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi.getCurrentQuietModeState());

    dndToggle.click();
    assertTrue(dndToggle.checked);
    assertTrue(mojoApi.getCurrentQuietModeState());

    // Click again will change the value.
    dndToggle.click();
    assertFalse(dndToggle.checked);
    assertFalse(mojoApi.getCurrentQuietModeState());
  });

  test('SetQuietMode being called correctly', async () => {
    createPage();
    const dndToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#doNotDisturbToggle');
    assertTrue(!!dndToggle);
    // Click the toggle button a certain count.
    const testCount = 3;

    flush();
    assertFalse(dndToggle.checked);

    await initializeObserver();
    flush();

    // Verify that every toggle click makes a call to setQuietMode and is
    // counted accordingly.
    for (let i = 0; i < testCount; i++) {
      dndToggle.click();
      await mojoApi.whenCalled('setQuietMode');
      setQuietModeCounter++;
      flush();
      assertEquals(i + 1, setQuietModeCounter);
    }
  });

  test('changing toggle with OnQuietModeChanged', async () => {
    createPage();
    const dndToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#doNotDisturbToggle');
    assertTrue(!!dndToggle);
    flush();
    assertFalse(dndToggle.checked);

    // Verify that calling onQuietModeChanged sets toggle value.
    // This is equivalent to changing the DoNotDisturb setting in quick
    // settings.
    await initializeObserver();
    flush();
    simulateQuickSettings(/** enable= */ true);
    await flushTasks();
    assertTrue(dndToggle.checked);
  });

  suite('App badging', () => {
    function makeFakePrefs(appBadgingEnabled = false): {[key: string]: any} {
      return {
        ash: {
          app_notification_badging_enabled: {
            key: 'ash.app_notification_badging_enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: appBadgingEnabled,
          },
        },
      };
    }

    teardown(() => {
      page.remove();
    });

    test('App badging toggle is visible', () => {
      createPage();
      const appBadgingToggle =
          page.shadowRoot!.querySelector('#appBadgingToggleButton');
      assertTrue(isVisible(appBadgingToggle));
    });

    test('Clicking the app badging button toggles the pref value', () => {
      createPage();
      page.prefs = makeFakePrefs(true);

      const appBadgingToggle =
          page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
              '#appBadgingToggleButton');
      assertTrue(!!appBadgingToggle);
      assertTrue(appBadgingToggle.checked);
      assertTrue(page.prefs['ash'].app_notification_badging_enabled.value);

      appBadgingToggle.click();
      assertFalse(appBadgingToggle.checked);
      assertFalse(page.prefs['ash'].app_notification_badging_enabled.value);

      appBadgingToggle.click();
      assertTrue(appBadgingToggle.checked);
      assertTrue(page.prefs['ash'].app_notification_badging_enabled.value);
    });
  });
});
