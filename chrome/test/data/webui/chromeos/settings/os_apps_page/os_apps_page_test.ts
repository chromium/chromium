// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';
import 'chrome://os-settings/os_settings.js';

import {ParentalControlsDialogAction, SettingsAndroidAppsSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {AndroidAppsBrowserProxyImpl, appNotificationHandlerMojom, CrDialogElement, createRouterForTesting, CrLinkRowElement, OsSettingsAppsPageElement, OsSettingsRoutes, Router, routes, routesMojom, setAppNotificationProviderForTesting, setAppParentalControlsProviderForTesting, settingMojom, SettingsDropdownMenuElement} from 'chrome://os-settings/os_settings.js';
import {Permission} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {createBoolPermission} from 'chrome://resources/cr_components/app_management/permission_util.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';
import {clearBody, hasStringProperty} from '../utils.js';

import {FakeAppNotificationHandler} from './app_notifications_page/fake_app_notification_handler.js';
import {FakeAppParentalControlsHandler} from './app_parental_controls_page/fake_app_parental_controls_handler.js';
import {TestAndroidAppsBrowserProxy} from './test_android_apps_browser_proxy.js';

const {Readiness} = appNotificationHandlerMojom;
type App = appNotificationHandlerMojom.App;
type ReadinessType = appNotificationHandlerMojom.Readiness;

const isRevampWayfindingEnabled =
    loadTimeData.getBoolean('isRevampWayfindingEnabled');
const isAppParentalControlsAvailable =
    loadTimeData.getBoolean('isAppParentalControlsFeatureAvailable');
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
        key: 'arc.enabled',
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
    on_device_app_controls: {
      pin: {
        key: 'on_device_app_controls.pin',
        type: chrome.settingsPrivate.PrefType.STRING,
        value: '',
      },
      setup_completed: {
        key: 'on_device_app_controls.setup_completed',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
  };
}

function setPrefs(restoreOption: number) {
  return {
    arc: {
      enabled: {
        key: 'arc.enabled',
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
  async function initPage(): Promise<void> {
    appsPage = document.createElement('os-settings-apps-page');
    appsPage.prefs = getFakePrefs();
    document.body.appendChild(appsPage);
    await flushTasks();
  }

  suiteSetup(() => {
    androidAppsBrowserProxy = new TestAndroidAppsBrowserProxy();
    AndroidAppsBrowserProxyImpl.setInstanceForTesting(androidAppsBrowserProxy);
  });

  setup(() => {
    loadTimeData.overrideValues({
      showOsSettingsAppNotificationsRow: true,
      isPlayStoreAvailable: true,
    });
    Router.getInstance().navigateTo(routes.APPS);
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

  test('Only App Management is shown', async () => {
    loadTimeData.overrideValues({
      shouldShowStartup: false,
      androidAppsVisible: false,
    });
    await initPage();

    assertTrue(isVisible(queryAppManagementRow()));
    assertNull(queryAndroidAppsRow());
    assertNull(queryAppsOnStartupRow());
  });

  test('Android Apps and App Management are shown', async () => {
    loadTimeData.overrideValues({
      shouldShowStartup: false,
      androidAppsVisible: true,
    });
    await initPage();

    assertTrue(isVisible(queryAppManagementRow()));
    assertTrue(isVisible(queryAndroidAppsRow()));
    assertNull(queryAppsOnStartupRow());
  });

  if (isRevampWayfindingEnabled) {
    test('On startup row does not exist in this page', async () => {
      loadTimeData.overrideValues({
        shouldShowStartup: true,
        androidAppsVisible: true,
      });
      await initPage();

      assertFalse(isVisible(queryAppsOnStartupRow()));
    });
  } else {
    test('Android Apps, On Startup, and App Management are shown', async () => {
      loadTimeData.overrideValues({
        shouldShowStartup: true,
        androidAppsVisible: true,
      });
      await initPage();

      assertTrue(isVisible(queryAppManagementRow()));
      assertTrue(isVisible(queryAndroidAppsRow()));
      assertTrue(isVisible(queryAppsOnStartupRow()));
      assertEquals(3, appsPage.get('onStartupOptions_').length);
    });
  }
});

suite('<os-apps-page> Subpage trigger focusing', () => {
  async function initPage(): Promise<void> {
    appsPage = document.createElement('os-settings-apps-page');
    appsPage.prefs = getFakePrefs();
    appsPage.androidAppsInfo = {
      playStoreEnabled: true,
      settingsAppAvailable: false,
    };
    document.body.appendChild(appsPage);
    await flushTasks();
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

  setup(() => {
    Router.getInstance().navigateTo(routes.APPS);
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
          await initPage();

          const subpageTrigger =
              appsPage.shadowRoot!.querySelector<HTMLButtonElement>(
                  triggerSelector);
          assertTrue(!!subpageTrigger);
          assertTrue(isVisible(subpageTrigger));

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

  test(
      'Returning from androidApps with playStore disabled focuses on button',
      async () => {
        await initPage();

        const subpageTrigger =
            appsPage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#androidApps .subpage-arrow');
        assertTrue(!!subpageTrigger);
        assertTrue(isVisible(subpageTrigger));

        // Sub-page trigger navigates to Detailed build info subpage
        subpageTrigger.click();
        assertEquals(
            routes.ANDROID_APPS_DETAILS, Router.getInstance().currentRoute);

        // Disable PlayStore
        appsPage.androidAppsInfo = {
          playStoreEnabled: false,
          settingsAppAvailable: false,
        };
        flush();

        // Navigate back
        const popStateEventPromise = eventToPromise('popstate', window);
        Router.getInstance().navigateToPreviousRoute();
        await popStateEventPromise;
        await waitAfterNextRender(appsPage);

        const arcEnableButton = appsPage.shadowRoot!.querySelector<HTMLElement>(
            '#androidApps #arcEnable');
        assertTrue(!!arcEnableButton);
        assertTrue(isVisible(arcEnableButton));

        assertEquals(
            arcEnableButton, appsPage.shadowRoot!.activeElement,
            '#arcEnable button should be focused.');
      });
});

suite('AppsPageTests', () => {
  let mojoApi: FakeAppNotificationHandler;
  let parentalControlsHandler: FakeAppParentalControlsHandler;

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

  setup(async () => {
    parentalControlsHandler = new FakeAppParentalControlsHandler();
    setAppParentalControlsProviderForTesting(parentalControlsHandler);

    Router.getInstance().navigateTo(routes.APPS);
    appsPage = document.createElement('os-settings-apps-page');
    document.body.appendChild(appsPage);
    await flushTasks();
  });

  teardown(() => {
    appsPage.remove();
    mojoApi.resetForTest();
    androidAppsBrowserProxy.reset();
    Router.getInstance().resetRouteForTesting();
  });

  suite('Main Page', () => {
    let fakeMetricsPrivate: FakeMetricsPrivate;

    setup(() => {
      fakeMetricsPrivate = new FakeMetricsPrivate();
      chrome.metricsPrivate = fakeMetricsPrivate;

      appsPage.prefs = getFakePrefs();
      appsPage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: false,
      };
      flush();
    });

    function queryAppNotificationsRow(): CrLinkRowElement|null {
      return appsPage.shadowRoot!.querySelector<CrLinkRowElement>(
          '#appNotificationsRow');
    }

    function queryParentalControlsRow(): HTMLElement|null {
      return appsPage.shadowRoot!.querySelector<HTMLElement>(
          '#appParentalControls');
    }

    async function initializeParentalControlsPin(pin: string) {
      await parentalControlsHandler.setUpPin(pin);
      appsPage.set('isParentalControlsSetupCompleted_', true);
      await flushTasks();
    }

    if (isRevampWayfindingEnabled) {
      test('App notification row displays helpful description', async () => {
        const rowLink = queryAppNotificationsRow();
        assertTrue(!!rowLink);
        assertTrue(isVisible(rowLink));
        assertEquals(
            'Manage app notifications, Do Not Disturb, and app badging',
            rowLink.subLabel);
      });

      test(
          'App notification row has same sublabel when Do Not Disturb is on',
          async () => {
            appsPage.set('isDndEnabled_', true);
            await flushTasks();

            const rowLink = queryAppNotificationsRow();
            assertTrue(!!rowLink);
            assertTrue(isVisible(rowLink));
            assertEquals(
                'Manage app notifications, Do Not Disturb, and app badging',
                rowLink.subLabel);
          });
    } else {
      test('App notification row displays number of apps', async () => {
        const rowLink = queryAppNotificationsRow();
        assertTrue(!!rowLink);
        assertTrue(isVisible(rowLink));
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

      test('App notification row shows when Do Not Disturb is on', async () => {
        appsPage.set('isDndEnabled_', true);
        await flushTasks();

        const rowLink = queryAppNotificationsRow();
        assertTrue(!!rowLink);
        assertTrue(isVisible(rowLink));
        assertEquals('Do Not Disturb enabled', rowLink.subLabel);
      });
    }

    test('Manage isolated web apps row', () => {
      const rowLink =
          appsPage.shadowRoot!.querySelector('#manageIsoalatedWebAppsRow');
      assertTrue(isVisible(rowLink));
    });

    if (isAppParentalControlsAvailable) {
      test(
          `Clicking set up and creating a PIN sets up parental controls and
           navigates to the parental controls subpage`,
          async () => {
            const parentalControlsRow = queryParentalControlsRow();
            assertTrue(!!parentalControlsRow);
            assertTrue(isVisible(parentalControlsRow));

            const setUpButton =
                parentalControlsRow.querySelector<HTMLElement>('cr-button');
            assertTrue(!!setUpButton);
            setUpButton.click();
            await flushTasks();

            const setupPinDialog =
                appsPage.shadowRoot!.querySelector<HTMLElement>('#setupPin');
            assertTrue(!!setupPinDialog);
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.SetUpControls',
                    ParentalControlsDialogAction.OPEN_DIALOG));

            // Simulate PIN entry.
            const pin = '123456';
            const setupPinKeyboard =
                setupPinDialog.shadowRoot!.getElementById('setupPinKeyboard');
            assertTrue(!!setupPinKeyboard);
            const pinKeyboard =
                setupPinKeyboard.shadowRoot!.getElementById('pinKeyboard');
            assertTrue(!!pinKeyboard);
            assertTrue(hasStringProperty(pinKeyboard, 'value'));
            pinKeyboard.value = pin;
            await flushTasks();

            const continuePinSetupButton =
                setupPinDialog.shadowRoot!
                    .querySelector<HTMLElement>('#dialog')!
                    .querySelector<HTMLElement>('.action-button');
            assertTrue(!!continuePinSetupButton);
            continuePinSetupButton.click();

            // Verify that the PIN keyboard has been reset.
            assertEquals('', pinKeyboard.value);

            // Re-enter the PIN to confirm it.
            pinKeyboard.value = pin;
            await flushTasks();

            assertTrue(!!continuePinSetupButton);
            continuePinSetupButton.click();
            await waitAfterNextRender(appsPage);

            // The subpage should be visible.
            assertEquals(
                routes.APP_PARENTAL_CONTROLS,
                Router.getInstance().currentRoute);
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.SetUpControls',
                    ParentalControlsDialogAction.FLOW_COMPLETED));
          });

      test(
          `Entering the correct PIN navigates to the parental controls subpage`,
          async () => {
            // Setup the initial PIN.
            const pin = '123456';
            await initializeParentalControlsPin(pin);

            const parentalControlsRow = queryParentalControlsRow();
            assertTrue(!!parentalControlsRow);
            assertTrue(isVisible(parentalControlsRow));

            // Click subpage arrow to navigate to the subpage.
            const subpageArrow = parentalControlsRow.querySelector<HTMLElement>(
                '.subpage-arrow');
            assertTrue(!!subpageArrow);
            subpageArrow.click();
            await flushTasks();

            const verifyPinDialog =
                appsPage.shadowRoot!.querySelector<HTMLElement>('#verifyPin');
            assertTrue(!!verifyPinDialog);
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.' +
                        'VerifyToEnterControlsPage',
                    ParentalControlsDialogAction.OPEN_DIALOG));

            // Simulate PIN entry.
            const verifyPinKeyboard =
                verifyPinDialog.shadowRoot!.getElementById('pinKeyboard');
            assertTrue(!!verifyPinKeyboard);
            assertTrue(hasStringProperty(verifyPinKeyboard, 'value'));
            verifyPinKeyboard.value = pin;
            await flushTasks();

            // Simulate pressing the enter key.
            const pinInput =
                verifyPinKeyboard.shadowRoot!.getElementById('pinInput');
            assertTrue(pinInput instanceof HTMLElement);
            pinInput.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'Enter', keyCode: 13}));
            await waitAfterNextRender(appsPage);

            // The subpage should be visible.
            assertEquals(
                routes.APP_PARENTAL_CONTROLS,
                Router.getInstance().currentRoute);
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.' +
                        'VerifyToEnterControlsPage',
                    ParentalControlsDialogAction.FLOW_COMPLETED));
          });

      test(
          `Entering an incorrect PIN surfaces an error and does not navigate
             to the parental controls subpage`,
          async () => {
            // Setup the initial PIN.
            const pin = '123456';
            await initializeParentalControlsPin(pin);

            const parentalControlsRow = queryParentalControlsRow();
            assertTrue(!!parentalControlsRow);
            assertTrue(isVisible(parentalControlsRow));

            // Click subpage arrow to navigate to the subpage.
            const subpageArrow = parentalControlsRow.querySelector<HTMLElement>(
                '.subpage-arrow');
            assertTrue(!!subpageArrow);
            subpageArrow.click();
            await flushTasks();

            const verifyPinDialog =
                appsPage.shadowRoot!.querySelector<HTMLElement>('#verifyPin');
            assertTrue(!!verifyPinDialog);
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.' +
                        'VerifyToEnterControlsPage',
                    ParentalControlsDialogAction.OPEN_DIALOG));

            // An error should not be visible in the dialog.
            const errorDiv =
                verifyPinDialog.shadowRoot!.getElementById('errorDiv');
            assertTrue(!!errorDiv);
            assertTrue(errorDiv.hasAttribute('invisible'));

            // Simulate incorrect PIN entry.
            const verifyPinKeyboard =
                verifyPinDialog.shadowRoot!.getElementById('pinKeyboard');
            assertTrue(!!verifyPinKeyboard);
            assertTrue(hasStringProperty(verifyPinKeyboard, 'value'));
            verifyPinKeyboard.value = '123457';
            await flushTasks();

            // Simulate pressing the enter key.
            const pinInput =
                verifyPinKeyboard.shadowRoot!.getElementById('pinInput');
            assertTrue(pinInput instanceof HTMLElement);
            pinInput.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'Enter', keyCode: 13}));
            await waitAfterNextRender(appsPage);

            // An error should be visible in the dialog.
            assertTrue(!!errorDiv);
            assertFalse(errorDiv.hasAttribute('invisible'));

            // The subpage should not be visible.
            assertEquals(routes.APPS, Router.getInstance().currentRoute);
          });

      test(
          `Toggling parental controls off and entering the correct PIN resets
             parental controls`,
          async () => {
            // Setup the initial PIN.
            const pin = '123456';
            await initializeParentalControlsPin(pin);

            const parentalControlsRow = queryParentalControlsRow();
            assertTrue(!!parentalControlsRow);
            assertTrue(isVisible(parentalControlsRow));

            // Click the toggle to disable parental controls.
            const toggle =
                parentalControlsRow.querySelector<HTMLElement>('cr-toggle');
            assertTrue(!!toggle);
            toggle.click();
            await flushTasks();

            const disableDialog =
                appsPage.shadowRoot!.querySelector<HTMLElement>(
                    '#disableDialog');
            assertTrue(!!disableDialog);
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.' +
                        'VerifyToDisableControls',
                    ParentalControlsDialogAction.OPEN_DIALOG));

            // Simulate PIN entry.
            const disablePinKeyboard =
                disableDialog.shadowRoot!.getElementById('pinKeyboard');
            assertTrue(!!disablePinKeyboard);
            assertTrue(hasStringProperty(disablePinKeyboard, 'value'));
            disablePinKeyboard.value = pin;

            // Simulate pressing the enter key.
            const pinInput =
                disablePinKeyboard.shadowRoot!.getElementById('pinInput');
            assertTrue(pinInput instanceof HTMLElement);
            pinInput.dispatchEvent(
                new KeyboardEvent('keydown', {key: 'Enter', keyCode: 13}));
            await waitAfterNextRender(appsPage);

            const setUpButton =
                parentalControlsRow.querySelector<HTMLElement>('cr-button');
            assertTrue(!!setUpButton);
            assertTrue(isVisible(setUpButton));
            assertEquals(
                1,
                fakeMetricsPrivate.countMetricValue(
                    'ChromeOS.OnDeviceControls.DialogAction.' +
                        'VerifyToDisableControls',
                    ParentalControlsDialogAction.FLOW_COMPLETED));
          });

      test(
          'Searching parental controls deep links to parental controls row',
          async () => {
            const parentalControlsSettingId =
                settingMojom.Setting.kAppParentalControls.toString();
            const params = new URLSearchParams();
            params.append('settingId', parentalControlsSettingId);
            Router.getInstance().navigateTo(routes.APPS, params);

            const parentalControlsRow = queryParentalControlsRow();
            assertTrue(!!parentalControlsRow);
            assertTrue(isVisible(parentalControlsRow));

            const setUpButton =
                parentalControlsRow.querySelector<HTMLElement>('cr-button');
            assertTrue(!!setUpButton);
            assertTrue(isVisible(setUpButton));
            await waitAfterNextRender(setUpButton);
            assertEquals(setUpButton, getDeepActiveElement());
            await flushTasks();

            // Setup initial PIN.
            await initializeParentalControlsPin('123456');

            Router.getInstance().navigateTo(routes.APPS, params);
            const subpageArrow =
                parentalControlsRow.querySelector('cr-icon-button');
            assertTrue(!!subpageArrow);
            assertTrue(isVisible(subpageArrow));
            await waitAfterNextRender(subpageArrow);
            assertEquals(subpageArrow, getDeepActiveElement());
          });
    }

    if (!isAppParentalControlsAvailable) {
      test('Parental controls row not visible when feature off', () => {
        const parentalControlsRow = queryParentalControlsRow();
        assertNull(parentalControlsRow);
      });
    }

    test('Clicking enable button enables ARC', () => {
      const androidApps = appsPage.shadowRoot!.querySelector('#androidApps');
      assertTrue(!!androidApps);
      const button = androidApps.querySelector<HTMLButtonElement>('#arcEnable');
      assertTrue(!!button);
      assertTrue(isVisible(button));
      assertNull(androidApps.querySelector('.subpage-arrow'));

      button.click();
      flush();
      assertTrue(appsPage.prefs.arc.enabled.value);

      appsPage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertTrue(isVisible(androidApps.querySelector('.subpage-arrow')));
    });

    // On startup row does not exist in the apps page under the revamp.
    if (!isRevampWayfindingEnabled) {
      test('On startup dropdown menu', () => {
        const getPrefValue = () => {
          const element =
              appsPage.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
                  '#onStartupDropdown');
          assertTrue(!!element);
          return element.pref!.value;
        };

        appsPage.prefs = setPrefs(1);
        flush();
        assertEquals(1, getPrefValue());

        appsPage.prefs = setPrefs(2);
        flush();
        assertEquals(2, getPrefValue());

        appsPage.prefs = setPrefs(3);
        flush();
        assertEquals(3, getPrefValue());
      });

      test('Deep link to On startup dropdown menu', async () => {
        const SETTING_ID_703 =
            settingMojom.Setting.kRestoreAppsAndPages.toString();
        const params = new URLSearchParams();
        params.append('settingId', SETTING_ID_703);
        Router.getInstance().navigateTo(routes.APPS, params);

        const deepLinkElement =
            appsPage.shadowRoot!.querySelector('#onStartupDropdown')!
                .shadowRoot!.querySelector<HTMLElement>('#dropdownMenu');
        assertTrue(!!deepLinkElement);
        assertTrue(isVisible(deepLinkElement));
        await waitAfterNextRender(deepLinkElement);
        assertEquals(
            deepLinkElement, getDeepActiveElement(),
            `On startup dropdown menu should be focused for settingId=${
                SETTING_ID_703}.`);
      });
    }

    test('Deep link to manage android prefs', async () => {
      // Simulate showing manage apps link
      appsPage.set('isPlayStoreAvailable_', false);
      flush();

      const SETTING_ID_700 =
          settingMojom.Setting.kManageAndroidPreferences.toString();
      const params = new URLSearchParams();
      params.append('settingId', SETTING_ID_700);
      Router.getInstance().navigateTo(routes.APPS, params);

      const deepLinkElement = appsPage.shadowRoot!.querySelector('#manageApps')!
                                  .shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!deepLinkElement);
      assertTrue(isVisible(deepLinkElement));
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
        appsPage.shadowRoot!.querySelector<HTMLButtonElement>('#arcEnable');
      assertTrue(!!deepLinkElement);
      assertTrue(isVisible(deepLinkElement));
      await waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          `Turn on play store button should be focused for settingId=${
              SETTING_ID_702}.`);
    });

    // TODO(crbug.com/40099962): Test that setting playStoreEnabled to false
    // navigates back to the main apps section.
  });

  suite('Android apps subpage', () => {
    let subpage: SettingsAndroidAppsSubpageElement;

    function queryManageAppsElement(): HTMLElement|null {
      return subpage.shadowRoot!.querySelector('#manageApps');
    }

    setup(() => {
      preliminarySetupForAndroidAppsSubpage(/*loadTimeDataOverrides=*/ null);

      // Because we can't simulate the loadTimeData value androidAppsVisible,
      // these route doesn't exist for tests. Add them in for testing.
      if (!routes.ANDROID_APPS_DETAILS) {
        routes.ANDROID_APPS_DETAILS = routes.APPS.createChild(
            '/' + routesMojom.GOOGLE_PLAY_STORE_SUBPAGE_PATH);
      }
      if (!routes.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES) {
        routes.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES =
            routes.ANDROID_APPS_DETAILS.createChild(
                '/' + routesMojom.ARC_VM_USB_PREFERENCES_SUBPAGE_PATH);
      }

      subpage = document.createElement('settings-android-apps-subpage');
      document.body.appendChild(subpage);

      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };

      flush();
    });

    teardown(() => {
      clearBody();
      subpage.remove();
    });

    test('Sanity', () => {
      assertTrue(isVisible(subpage.shadowRoot!.querySelector('#remove')));
      assertNull(queryManageAppsElement());
    });

    test('ManageAppsUpdate', () => {
      assertNull(queryManageAppsElement());
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      assertTrue(isVisible(queryManageAppsElement()));

      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: false,
      };
      flush();
      assertNull(queryManageAppsElement());
    });

    test('ManageAppsOpenRequest', async () => {
      subpage.androidAppsInfo = {
        playStoreEnabled: true,
        settingsAppAvailable: true,
      };
      flush();
      const button = queryManageAppsElement();
      assertTrue(!!button);
      assertTrue(isVisible(button));

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

      const removeButton =
          subpage.shadowRoot!.querySelector<HTMLElement>('#remove > cr-button');
      assertTrue(!!removeButton);
      assertTrue(isVisible(removeButton));
      removeButton.click();
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
      assertTrue(isVisible(queryManageAppsElement()));
    });

    test('Can open app settings without Play Store', async () => {
      subpage.prefs = {arc: {enabled: {value: true}}};
      subpage.androidAppsInfo = {
        playStoreEnabled: false,
        settingsAppAvailable: true,
      };
      flush();

      const button = queryManageAppsElement();
      assertTrue(!!button);
      assertTrue(isVisible(button));

      button.click();
      flush();
      await androidAppsBrowserProxy.whenCalled('showAndroidAppsSettings');
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

      const deepLinkElement =
          queryManageAppsElement()!.shadowRoot!.querySelector('cr-icon-button');
      assertTrue(!!deepLinkElement);
      assertTrue(isVisible(deepLinkElement));
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
      assertTrue(isVisible(deepLinkElement));
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
      assertTrue(isVisible(
          subpage.shadowRoot!.querySelector('#manageArcvmShareUsbDevices')));
    });

    test('ManageUsbDevice returning navigation sets focus', async () => {
      subpage.isArcVmManageUsbAvailable = true;
      Router.getInstance().navigateTo(routes.ANDROID_APPS_DETAILS);

      const subpageLink = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
          '#manageArcvmShareUsbDevices');
      assertTrue(!!subpageLink);
      assertTrue(isVisible(subpageLink));

      subpageLink.click();
      assertEquals(
          routes.ANDROID_APPS_DETAILS_ARC_VM_SHARED_USB_DEVICES,
          Router.getInstance().currentRoute);

      // Navigate back
      const popStateEventPromise = eventToPromise('popstate', window);
      Router.getInstance().navigateToPreviousRoute();
      await popStateEventPromise;
      await waitAfterNextRender(subpage);

      assertEquals(
          subpageLink, subpage.shadowRoot!.activeElement,
          `#manageArcvmShareUsbDevices should be focused.`);
    });

    if (isRevampWayfindingEnabled) {
      test(
          'Open Google Play link row appears and once clicked, ' +
              'opens the play store app',
          async () => {
            const row = subpage.shadowRoot!.querySelector<HTMLButtonElement>(
                '#openGooglePlayRow');
            assertTrue(!!row);
            assertTrue(isVisible(row));

            row.click();
            flush();
            await androidAppsBrowserProxy.whenCalled('showPlayStoreApps');
          });
    } else {
      test('Open Google Play link row does not appear.', () => {
        const row = subpage.shadowRoot!.querySelector('#openGooglePlayRow');
        assertFalse(isVisible(row));
      });
    }
  });
});
