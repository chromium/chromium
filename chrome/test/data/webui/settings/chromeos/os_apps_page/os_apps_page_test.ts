// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://os-settings/os_settings.js';

import {SettingsAndroidAppsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {AndroidAppsBrowserProxyImpl, appNotificationHandlerMojom, CrDialogElement, createRouterForTesting, CrLinkRowElement, OsSettingsAppsPageElement, OsSettingsRoutes, Router, routes, routesMojom, setAppNotificationProviderForTesting, settingMojom, SettingsDropdownMenuElement} from 'chrome://os-settings/os_settings.js';
import {Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {disableAnimationsAndTransitions} from 'chrome://webui-test/test_api.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeAppNotificationHandler} from './app_notifications_page/fake_app_notification_handler.js';
import {TestAndroidAppsBrowserProxy} from './test_android_apps_browser_proxy.js';

const {Readiness} = appNotificationHandlerMojom;
type App = appNotificationHandlerMojom.App;
type ReadinessType = appNotificationHandlerMojom.Readiness;

let appsPage: OsSettingsAppsPageElement;
let androidAppsBrowserProxy: TestAndroidAppsBrowserProxy;

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

function getFakePrefs() {
  return {
    arc: {
      enabled: {
        key: 'arc.enabledd',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 2,
      },
    },
  };
}

function setPrefs(restoreOption: number) {
  return {
    arc: {
      enabled: {
        key: 'arc.enabledd',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: restoreOption,
      },
    },
  };
}

function preliminarySetupForAndroidAppsSubpage(
    loadTimeDataOverrides: {[key: string]: any}|null): void {
  if (loadTimeDataOverrides) {
    loadTimeData.overrideValues(loadTimeDataOverrides);
  }

  androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
  AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);

  disableAnimationsAndTransitions();
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

suite('<os-apps-page> available settings rows', () => {
  function initPage(): void {
    loadTimeData.overrideValues({isPlayStoreAvailable: true});
    appsPage = document.createElement('os-settings-apps-page');
    appsPage.prefs = getFakePrefs();
    document.body.appendChild(appsPage);
    flush();
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({showOsSettingsAppNotificationsRow: true});
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);
  });

  teardown(() => {
    appsPage.remove();
    androidAppsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  const queryAndroidAppsRow = () =>
      appsPage.shadowRoot!.querySelector('#androidApps');
  const queryAppManagementRow = () =>
      appsPage.shadowRoot!.querySelector('#appManagementRow');
  const queryAppsOnStartupRow = () =>
      appsPage.shadowRoot!.querySelector('#onStartupDropdown');

  test('Only App Management is shown', () => {
    loadTimeData.overrideValues({
      shouldShowStartup: false,
      androidAppsVisible: false,
    });
    initPage();

    assertTrue(!!queryAppManagementRow());
    assertNull(queryAndroidAppsRow());
    assertNull(queryAppsOnStartupRow());
  });

  test('Android Apps and App Management are shown', () => {
    loadTimeData.overrideValues({
      shouldShowStartup: false,
      androidAppsVisible: true,
    });
    initPage();

    assertTrue(!!queryAppManagementRow());
    assertTrue(!!queryAndroidAppsRow());
    assertNull(queryAppsOnStartupRow());
  });

  test('Android Apps, On Startup, and App Management are shown', () => {
    loadTimeData.overrideValues({
      shouldShowStartup: true,
      androidAppsVisible: true,
    });
    initPage();

    assertTrue(!!queryAppManagementRow());
    assertTrue(!!queryAndroidAppsRow());
    assertTrue(!!queryAppsOnStartupRow());
    assertEquals(3, appsPage.get('onStartupOptions_').length);
  });
});

suite('<os-apps-page> Subpage trigger focusing', () => {
  function initPage(): void {
    Router.getInstance().navigateTo(routes.APPS);
    appsPage = document.createElement('os-settings-apps-page');
    appsPage.prefs = getFakePrefs();
    appsPage.androidAppsInfo = {
      playStoreEnabled: true,
      settingsAppAvailable: false,
    };
    document.body.appendChild(appsPage);
    flush();
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      androidAppsVisible: true,
      showOsSettingsAppNotificationsRow: true,
    });

    // Reinitialize Router and routes based on load time data
    const testRouter = createRouterForTesting();
    Router.resetInstanceForTesting(testRouter);

    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);
  });

  teardown(() => {
    appsPage.remove();
    androidAppsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  const subpageTriggerData: SubpageTriggerData[] = [
    {
      triggerSelector: '#appManagementRow',
      routeName: 'APP_MANAGEMENT',
    },
    {
      triggerSelector: '#appNotificationsRow',
      routeName: 'APP_NOTIFICATIONS',
    },
    {
      triggerSelector: '#androidApps .subpage-arrow',
      routeName: 'ANDROID_APPS_DETAILS',
    },
  ];
  subpageTriggerData.forEach(({triggerSelector, routeName}) => {
    test(
        `${routeName} subpage trigger is focused when returning from subpage`,
        async () => {
          initPage();

          const subpageTrigger =
              appsPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  triggerSelector);
          assertTrue(!!subpageTrigger);

          // Sub-page trigger navigates to Detailed build info subpage
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(appsPage);

          assertEquals(
              subpageTrigger, appsPage.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });
});

suite('AppsPageTests', () => {
  let mojoApi: FakeAppNotificationHandler;

  function simulateNotificationAppChanged(app: App): void {
    mojoApi.getObserverRemote().onNotificationAppChanged(app);
  }

  suiteSetup(() => {
    loadTimeData.overrideValues({
      showOsSettingsAppNotificationsRow: true,
      isPlayStoreAvailable: true,
      androidAppsVisible: true,
      showManageIsolatedWebAppsRow: true,
    });
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);

    mojoApi = new FakeAppNotificationHandler();
    setAppNotificationProviderForTesting(mojoApi);
  });

  setup(() => {
    appsPage = document.createElement('os-settings-apps-page');
    document.body.appendChild(appsPage);
    disableAnimationsAndTransitions();
  });

  teardown(() => {
    mojoApi.resetForTest();
    appsPage.remove();
    androidAppsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  suite('Main Page', () => {
    setup(() => {
      appsPage.prefs = getFakePrefs();
      appsPage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: false,
      };
      flush();
    });

    test('App notification row', async () => {
      const rowLink = appsPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#appNotificationsRow');
      assertTrue(!!rowLink);
      // Test default is to have 0 apps.
      assertEquals('0 apps', rowLink.subLabel);

      const permission1 = createBoolPermission(
          /**id=*/ 1,
          /**value=*/ false, /**is_managed=*/ false);
      const permission2 = createBoolPermission(
          /**id=*/ 2,
          /**value=*/ true, /**is_managed=*/ false);
      const app1 = createApp('1', 'App1', permission1);
      const app2 = createApp('2', 'App2', permission2);

      simulateNotificationAppChanged(app1);
      simulateNotificationAppChanged(app2);
      await flushTasks();

      assertEquals('2 apps', rowLink.subLabel);

      // Simulate an uninstalled app.
      const app3 =
          createApp('2', 'App2', permission2, Readiness.kUninstalledByUser);
      simulateNotificationAppChanged(app3);
      await flushTasks();
      assertEquals('1 apps', rowLink.subLabel);
    });

    test('Manage isolated web apps row', () => {
      const rowLink =
          appsPage.shadowRoot!.querySelector('#manageIsoalatedWebAppsRow');
      assertTrue(!!rowLink);
    });

    test('Clicking enable button enables ARC', () => {
      const button =
          appsPage.shadowRoot!.querySelector<HTMLButtonElement>('#enable');
      assertTrue(!!button);
      assertNull(appsPage.shadowRoot!.querySelector('.subpage-arrow'));

      button.click();
      flush();
      assertTrue(appsPage.prefs.arc.enabled.value);

      appsPage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertTrue(!!appsPage.shadowRoot!.querySelector('.subpage-arrow'));
    });

    test('On startup dropdown menu', () => {
      const getPref = () => {
        const element =
            appsPage.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
                '#onStartupDropdown');
        assertTrue(!!element);
        const pref = element.pref;
        assertTrue(!!pref);
        return pref;
      };

      appsPage.prefs = setPrefs(1);
      flush();
      assertEquals(1, getPref().value);

      appsPage.prefs = setPrefs(2);
      flush();
      assertEquals(2, getPref().value);

      appsPage.prefs = setPrefs(3);
      flush();
      assertEquals(3, getPref().value);
    });

    test('Deep link to On startup dropdown menu', async () => {
      const SETTING_ID_703 =
          settingMojom.Setting.kRestoreAppsAndPages.toString();
      const params = new URLSearchParams();
      params.append('settingId', SETTING_ID_703);
      Router.getInstance().navigateTo(routes.APPS, params);

      const element = appsPage.shadowRoot!.querySelector('#onStartupDropdown');
      assertTrue(!!element);
      const deepLinkElement =
          element.shadowRoot!.querySelector<HTMLElement>('#dropdownMenu');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `On startup dropdown menu should be focused for settingId=${
              SETTING_ID_703}.`);
    });

    test('Deep link to manage android prefs', async () => {
      // Simulate showing manage apps link
      appsPage.set('isPlayStoreAvailable_', false);
      flush();

      const SETTING_ID_700 =
          settingMojom.Setting.kManageAndroidPreferences.toString();
      const params = new URLSearchParams();
      params.append('settingId', SETTING_ID_700);
      Router.getInstance().navigateTo(routes.APPS, params);

      const element = appsPage.shadowRoot!.querySelector('#manageApps');
      assertTrue(!!element);
      const deepLinkElement =
          element.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Manage android prefs button should be focused for settingId=${
              SETTING_ID_700}.`);
    });

    test('Deep link to turn on Play Store', async () => {
      const SETTING_ID_702 = settingMojom.Setting.kTurnOnPlayStore.toString();
      const params = new URLSearchParams();
      params.append('settingId', SETTING_ID_702);
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement =
          appsPage.shadowRoot!.querySelector<HTMLButtonElement>('#enable');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Turn on play store button should be focused for settingId=${
              SETTING_ID_702}.`);
    });

    // TODO(crbug.com/1006662): Test that setting playStoreEnabled to false
    // navigates back to the main apps section.
  });

  suite('Android apps subpage', () => {
    let subpage: SettingsAndroidAppsSubpageElement;

    setup(() => {
      preliminarySetupForAndroidAppsSubpage(/*loadTimeDataOverrides=*/ null);

      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);

      // Because we can't simulate the loadTimeData value androidAppsVisible,
      // this route doesn't exist for tests. Add it in for testing.
      if (!routes.ANDROID_APPS_DETAILS) {
        routes.ANDROID_APPS_DETAILS = routes.APPS.createChild(
            '/' + routesMojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH);
      }

      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };

      flush();
    });

    teardown(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      subpage.remove();
    });

    test('Sanity', () => {
      assertTrue(!!subpage.shadowRoot!.querySelector('#remove'));
      assertNull(subpage.shadowRoot!.querySelector('#manageApps'));
    });

    test('ManageAppsUpdate', () => {
      assertNull(subpage.shadowRoot!.querySelector('#manageApps'));
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      assertTrue(!!subpage.shadowRoot!.querySelector('#manageApps'));

      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertNull(subpage.shadowRoot!.querySelector('#manageApps'));
    });

    test('ManageAppsOpenRequest', async () => {
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      const button =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      flush();
      await promise;
    });

    test('Disable', () => {
      const dialog = subpage.shadowRoot!.querySelector<CrDialogElement>(
          '#confirmDisableDialog');
      assertTrue(!!dialog);
      assertFalse(dialog.open);

      const remove = subpage.shadowRoot!.querySelector('#remove');
      assertTrue(!!remove);

      const button = remove.querySelector('cr-button');
      assertTrue(!!button);
      button.click();
      flush();

      assertTrue(dialog.open);
      dialog.close();
    });

    test('ARC enabled by policy', () => {
      subpage.prefs = {
        arc: {
          enabled: {
            value: true,
            enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
          },
        },
      };
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();

      assertNull(subpage.shadowRoot!.querySelector('#remove'));
      assertTrue(!!subpage.shadowRoot!.querySelector('#manageApps'));
    });

    test('Can open app settings without Play Store', async () => {
      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      flush();

      const button =
          subpage.shadowRoot!.querySelector<HTMLButtonElement>('#manageApps');
      assertTrue(!!button);
      const promise =
          androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
      button.click();
      flush();
      await promise;
    });

    test('Deep link to manage android prefs - subpage', async () => {
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      flush();

      const SETTING_ID_700 =
          settingMojom.Setting.kManageAndroidPreferences.toString();
      const params = new URLSearchParams();
      params.append('settingId', SETTING_ID_700);
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS, params);

      const element = subpage.shadowRoot!.querySelector('#manageApps');
      assertTrue(!!element);
      const deepLinkElement =
          element.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Manage android prefs button should be focused for settingId=${
              SETTING_ID_700}.`);
    });

    test('Deep link to remove play store', async () => {
      const SETTING_ID_701 = settingMojom.Setting.kRemovePlayStore.toString();
      const params = new URLSearchParams();
      params.append('settingId', SETTING_ID_701);
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS, params);

      const deepLinkElement =
          subpage.shadowRoot!.querySelector<HTMLElement>('#remove cr-button');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Remove play store button should be focused for settingId=${
              SETTING_ID_701}.`);
    });

    test('ManageUsbDevice', () => {
      // ARCVM is not enabled
      subpage.isArcVmManageUsbAvailable = false;
      flush();
      assertNull(
          subpage.shadowRoot!.querySelector('#manageArcvmShareUsbDevices'));

      // ARCMV is enabled
      subpage.isArcVmManageUsbAvailable = true;
      flush();
      assertTrue(
          !!subpage.shadowRoot!.querySelector('#manageArcvmShareUsbDevices'));
    });
  });

  suite('with OsSettingsRevampWayfinding feature enabled', () => {
    let subpage: SettingsAndroidAppsSubpageElement;

    setup(() => {
      const loadTimeDataOverrides = {
        isRevampWayfindingEnabled: true,
      };

      preliminarySetupForAndroidAppsSubpage(loadTimeDataOverrides);

      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);

      flush();
    });

    teardown(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      subpage.remove();
    });

    test(
        'Open Google Play link row appears and once clicked, ' +
            'opens the play store app',
        async () => {
          const row = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
              '#openGooglePlayRow');
          assertTrue(!!row);

          row.click();
          flush();
          await androidAppsBrowserProxy.whenCalled('showPlayStoreApps');
        });
  });

  suite('with OsSettingsRevampWayfinding feature disabled', () => {
    let subpage: SettingsAndroidAppsSubpageElement;

    setup(() => {
      const loadTimeDataOverrides = {
        isRevampWayfindingEnabled: false,
      };

      preliminarySetupForAndroidAppsSubpage(loadTimeDataOverrides);

      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);

      flush();
    });

    teardown(() => {
      document.body.innerHTML = window.trustedTypes!.emptyHTML;
      subpage.remove();
    });

    test('Open Google Play link row does not appear.', () => {
      const row = subpage.shadowRoot!.querySelector('#openGooglePlayRow');
      assertFalse(isVisible(row));
    });
  });
});
