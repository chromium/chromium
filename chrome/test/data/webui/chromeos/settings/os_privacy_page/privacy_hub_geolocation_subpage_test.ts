// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PrivacyHubBrowserProxyImpl, SettingsPrivacyHubGeolocationSubpage} from 'chrome://os-settings/lazy_load.js';
import {appPermissionHandlerMojom, ControlledButtonElement, ControlledRadioButtonElement, CrDialogElement, CrLinkRowElement, CrTooltipIconElement, GeolocationAccessLevel, OpenWindowProxyImpl, Router, routes, ScheduleType, setAppPermissionProviderForTesting, SettingsPrivacyHubSystemServiceRow} from 'chrome://os-settings/os_settings.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {DomRepeat} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertLT, assertNotReached, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';
import {createApp, createFakeMetricsPrivate, getSystemServiceName, getSystemServicePermissionText, getSystemServicesFromSubpage} from './privacy_hub_app_permission_test_util.js';
import {TestPrivacyHubBrowserProxy} from './test_privacy_hub_browser_proxy.js';

type App = appPermissionHandlerMojom.App;

suite('<settings-privacy-hub-geolocation-subpage>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let metrics: FakeMetricsPrivate;
  let privacyHubGeolocationSubpage: SettingsPrivacyHubGeolocationSubpage;
  let privacyHubBrowserProxy: TestPrivacyHubBrowserProxy;
  let openWindowProxy: TestOpenWindowProxy;

  async function initPage() {
    privacyHubGeolocationSubpage =
        document.createElement('settings-privacy-hub-geolocation-subpage');
    const prefs = {
      ash: {
        user: {
          geolocation_access_level: {
            key: 'ash.user.geolocation_access_level',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: GeolocationAccessLevel.ALLOWED,
          },
        },
        dark_mode: {
          schedule_type: {
            key: 'ash.user.dark_mode.schedule_type',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ScheduleType.SUNSET_TO_SUNRISE,
          },
        },
        night_light: {
          schedule_type: {
            key: 'ash.night_light.schedule_type',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: ScheduleType.SUNSET_TO_SUNRISE,
          },
        },
      },
      generated: {
        resolve_timezone_by_geolocation_on_off: {
          key: 'generated.resolve_timezone_by_geolocation_on_off',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
      settings: {
        ambient_mode: {
          enabled: {
            value: true,
          },
        },
      },
    };
    privacyHubGeolocationSubpage.prefs = prefs;
    document.body.appendChild(privacyHubGeolocationSubpage);
    await flushTasks();
  }


  setup(() => {
    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);
    metrics = createFakeMetricsPrivate();
    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);

    Router.getInstance().navigateTo(routes.PRIVACY_HUB_GEOLOCATION);
  });

  teardown(() => {
    privacyHubGeolocationSubpage.remove();
    Router.getInstance().resetRouteForTesting();
    openWindowProxy.reset();
  });

  function histogram(): string {
    return 'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.SystemSettings';
  }

  function getGeolocationLabel(): string {
    return privacyHubGeolocationSubpage.shadowRoot!
        .querySelector<HTMLDivElement>('#geolocationStatus')!.innerText;
  }

  function getGeolocationSubLabel(): string {
    return privacyHubGeolocationSubpage.shadowRoot!
        .querySelector<HTMLDivElement>(
            '#geolocationStatusDescription')!.innerText;
  }

  async function setGeolocationAccessLevelPref(
      accessLevel: GeolocationAccessLevel) {
    privacyHubGeolocationSubpage.prefs.ash.user.geolocation_access_level.value =
        accessLevel;
    privacyHubGeolocationSubpage.notifyPath(
        'prefs.ash.user.geolocation_access_level', accessLevel);
    await flushTasks();
  }

  function getGeolocationAccessLevel(): GeolocationAccessLevel {
    const geolocationAccessLevel = getGeolocationLabel();
    assertTrue(!!geolocationAccessLevel);

    switch (geolocationAccessLevel) {
      case privacyHubGeolocationSubpage.i18n(
          'geolocationAccessLevelDisallowed'):
        return GeolocationAccessLevel.DISALLOWED;
      case privacyHubGeolocationSubpage.i18n('geolocationAccessLevelAllowed'):
        return GeolocationAccessLevel.ALLOWED;
      case privacyHubGeolocationSubpage.i18n(
          'geolocationAccessLevelOnlyAllowedForSystem'):
        return GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM;
    }

    assertNotReached('Invalid GeolocationAccessLevel value detected');
  }

  function getNoAppHasAccessTextSection(): HTMLElement|null {
    return privacyHubGeolocationSubpage.shadowRoot!.querySelector(
        '#noAppHasAccessText');
  }

  function getAppList(): DomRepeat|null {
    return privacyHubGeolocationSubpage.shadowRoot!.querySelector('#appList');
  }

  function getNthAppName(i: number): string|null {
    const appPermissionRows =
        privacyHubGeolocationSubpage.shadowRoot!.querySelectorAll(
            '#appsSection > div > settings-privacy-hub-app-permission-row');
    assertLT(i, appPermissionRows.length);

    return appPermissionRows[i]!.shadowRoot!.querySelector(
                                                '#appName')!.textContent;
  }

  interface TextOptions {
    notConfiguredText: string;
    allowedText: string;
    blockedText: string;
  }

  function checkService(
      systemService: SettingsPrivacyHubSystemServiceRow, expectedName: string,
      isConfiguredToUseGeolocation: boolean,
      expectedDescriptions: TextOptions) {
    // Check  service name.
    assertEquals(expectedName, getSystemServiceName(systemService));

    // Check subtext:
    // Check when the system service is not configured to use geolocation (e.g.
    // time zone is selected from the static list).
    if (!isConfiguredToUseGeolocation) {
      assertEquals(
          expectedDescriptions['notConfiguredText'],
          getSystemServicePermissionText(systemService));
      return;
    }
    // Check when the system service is using geolocation.
    switch (getGeolocationAccessLevel()) {
      case GeolocationAccessLevel.DISALLOWED:
        assertEquals(
            expectedDescriptions['blockedText'],
            getSystemServicePermissionText(systemService));
        break;
      case GeolocationAccessLevel.ALLOWED:
        // Falls through to ONLY_ALLOWED_FOR_SYSTEM
      case GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM:
        assertEquals(
            expectedDescriptions['allowedText'],
            getSystemServicePermissionText(systemService));
        break;
    }
  }

  async function setAutomaticTimeZoneEnabled(enabled: boolean) {
    privacyHubGeolocationSubpage.set(
        'prefs.generated.resolve_timezone_by_geolocation_on_off.value',
        enabled);
    await flushTasks();
  }

  async function setNightLightScheduleType(scheduleType: ScheduleType) {
    privacyHubGeolocationSubpage.set(
        'prefs.ash.night_light.schedule_type.value', scheduleType);
    await flushTasks();
  }

  async function setLocalWeatherEnabled(enabled: boolean) {
    privacyHubGeolocationSubpage.set(
        'prefs.settings.ambient_mode.enabled.value', enabled);
    await flushTasks();
  }

  async function setDarkThemeScheduleType(scheduleType: ScheduleType) {
    privacyHubGeolocationSubpage.set(
        'prefs.ash.dark_mode.schedule_type.value', scheduleType);
    await flushTasks();
  }


  async function checkServiceSection() {
    assertEquals(
        privacyHubGeolocationSubpage.i18n(
            'privacyHubSystemServicesSectionTitle'),
        privacyHubGeolocationSubpage.shadowRoot!
            .querySelector<HTMLElement>(
                '#systemServicesSectionTitle')!.innerText!.trim());

    const systemServices =
        getSystemServicesFromSubpage(privacyHubGeolocationSubpage);
    assertEquals(4, systemServices.length);

    for (const timeZoneAutomaticSetting of [true, false]) {
      await setAutomaticTimeZoneEnabled(timeZoneAutomaticSetting);

      checkService(
          systemServices[0]!,
          privacyHubGeolocationSubpage.i18n(
              'privacyHubSystemServicesAutomaticTimeZoneName'),
          timeZoneAutomaticSetting, {
            notConfiguredText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesGeolocationNotConfigured'),
            allowedText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesAllowedText'),
            blockedText: 'Blocked. Time zone is currently set to ' +
                'Test Time Zone' +
                ' and can only be updated manually.',
          });
    }

    const allScheduleTypes: ScheduleType[] =
        Object.values(ScheduleType)
            .filter(value => typeof value === 'number') as ScheduleType[];
    // Test Night Light
    for (const scheduleType of allScheduleTypes) {
      await setNightLightScheduleType(scheduleType as ScheduleType);

      checkService(
          systemServices[1]!,
          privacyHubGeolocationSubpage.i18n(
              'privacyHubSystemServicesSunsetScheduleName'),
          scheduleType === ScheduleType.SUNSET_TO_SUNRISE, {
            notConfiguredText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesGeolocationNotConfigured'),
            allowedText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesAllowedText'),
            blockedText:
                'Blocked. Schedule is currently set to 7:00AM - 8:00PM' +
                ' and can only be updated manually.',
          });
    }

    // Test Dark Theme
    for (const scheduleType of allScheduleTypes) {
      await setDarkThemeScheduleType(scheduleType as ScheduleType);

      checkService(
          systemServices[2]!,
          privacyHubGeolocationSubpage.i18n(
              'privacyHubSystemServicesDarkThemeName'),
          scheduleType === ScheduleType.SUNSET_TO_SUNRISE, {
            notConfiguredText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesGeolocationNotConfigured'),
            allowedText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesAllowedText'),
            blockedText:
                'Blocked. Schedule is currently set to 7:00AM - 8:00PM' +
                ' and can only be updated manually.',
          });
    }

    // Test Local Weather
    for (const localWeatherEnabled of [true, false]) {
      await setLocalWeatherEnabled(localWeatherEnabled);

      checkService(
          systemServices[3]!,
          privacyHubGeolocationSubpage.i18n(
              'privacyHubSystemServicesLocalWeatherName'),
          localWeatherEnabled, {
            notConfiguredText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesGeolocationNotConfigured'),
            allowedText: privacyHubGeolocationSubpage.i18n(
                'privacyHubSystemServicesAllowedText'),
            blockedText: 'Blocked',
          });
    }
  }

  test('Geolocation sub-label updates on location change', async () => {
    await initPage();

    // Check "Allowed"
    assertTrue(getGeolocationAccessLevel() === GeolocationAccessLevel.ALLOWED);
    assertEquals(
        privacyHubGeolocationSubpage.i18n('geolocationAccessLevelAllowed'),
        getGeolocationLabel());
    assertEquals(
        privacyHubGeolocationSubpage.i18n('geolocationAllowedModeDescription'),
        getGeolocationSubLabel());

    // Check "Allowed For System Services"
    setGeolocationAccessLevelPref(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertTrue(
        getGeolocationAccessLevel() ===
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertEquals(
        privacyHubGeolocationSubpage.i18n(
            'geolocationAccessLevelOnlyAllowedForSystem'),
        getGeolocationLabel());
    assertEquals(
        privacyHubGeolocationSubpage.i18n(
            'geolocationOnlyAllowedForSystemModeDescription'),
        getGeolocationSubLabel());

    // Check "Blocked for all"
    setGeolocationAccessLevelPref(GeolocationAccessLevel.DISALLOWED);
    assertTrue(
        getGeolocationAccessLevel() === GeolocationAccessLevel.DISALLOWED);
    assertEquals(
        privacyHubGeolocationSubpage.i18n('geolocationAccessLevelDisallowed'),
        getGeolocationLabel());
    assertEquals(
        privacyHubGeolocationSubpage.i18n('geolocationBlockedModeDescription'),
        getGeolocationSubLabel());
  });

  function getGeolocationSelectorDialog(): CrDialogElement|null {
    return privacyHubGeolocationSubpage.shadowRoot!
        .querySelector<CrDialogElement>('#systemGeolocationDialog');
  }

  async function selectGeolocationAccessLevelInDialog(
      accessLevel: GeolocationAccessLevel) {
    // Exhaustive list of radio button id's of the dialog mapped to its
    // corresponding  geolocation access level.
    const accessLevelToRadioButtonIdMap = {
      [GeolocationAccessLevel.ALLOWED]: 'systemGeolocationAccessAllowedForAll',
      [GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM]:
          'systemGeolocationAccessOnlyAllowedForSystem',
      [GeolocationAccessLevel.DISALLOWED]: 'systemGeolocationAccessBlocked',
    };
    assertEquals(
        GeolocationAccessLevel.MAX_VALUE + 1,
        Object.keys(accessLevelToRadioButtonIdMap).length);

    // Click change button.
    const changeGeolocationButton =
        privacyHubGeolocationSubpage.shadowRoot!.querySelector<CrButtonElement>(
            '#changeAccessButton');
    assertTrue(!!changeGeolocationButton, 'Change button is missing');
    changeGeolocationButton.click();
    await flushTasks();

    // Check that geolocation selector dialog appears.
    const geolocationSelectorDialog = getGeolocationSelectorDialog();
    assertTrue(
        !!geolocationSelectorDialog, 'Geolocation selector dialog is missing');

    // Check that the Dialog radio-buttons represent all access levels.
    const radioButtonList =
        geolocationSelectorDialog.querySelectorAll('controlled-radio-button');
    assertEquals(
        GeolocationAccessLevel.MAX_VALUE + 1, radioButtonList.length,
        'Radio buttons don\'t match with the access levels');

    // Select the intended radio button and click submit.
    const radioButton: ControlledRadioButtonElement =
        Array.from(radioButtonList)
            .find(
                (elem: ControlledRadioButtonElement) =>
                    elem.id === accessLevelToRadioButtonIdMap[accessLevel])!;
    radioButton.click();
    await flushTasks();
    const confirmButton: CrButtonElement =
        geolocationSelectorDialog.querySelector<CrButtonElement>(
            '#confirmButton')!;
    confirmButton.click();
    await flushTasks();

    // Check that the dialog disappears.
    assertNull(
        getGeolocationSelectorDialog(),
        'Geolocation selector dialog is not dismissed');
  }

  test('Select geolocation access level from dialog', async () => {
    await initPage();

    // Geolocation selector dialog should be hidden.
    assertNull(getGeolocationSelectorDialog());

    for (const accessLevel of Object.values(GeolocationAccessLevel)
             .filter(v => typeof v === 'number')) {
      await selectGeolocationAccessLevelInDialog(accessLevel);
      assertEquals(
          accessLevel,
          privacyHubGeolocationSubpage
              .getPref('ash.user.geolocation_access_level')
              .value,
          'Preference didn\'t update');
    }
  });

  test('App list displayed when geolocation allowed', async () => {
    await initPage();
    assertEquals(
        privacyHubGeolocationSubpage.i18n('privacyHubAppsSectionTitle'),
        privacyHubGeolocationSubpage.shadowRoot!
            .querySelector<HTMLElement>(
                '#appsSectionTitle')!.innerText!.trim());
    assertTrue(!!getAppList());
    assertNull(getNoAppHasAccessTextSection());
  });

  test('App list not displayed when geolocation not allowed', async () => {
    await initPage();
    // Disable geolocation access.
    setGeolocationAccessLevelPref(GeolocationAccessLevel.DISALLOWED);

    assertNull(getAppList());
    assertTrue(!!getNoAppHasAccessTextSection());
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noAppCanUseGeolocationText'),
        getNoAppHasAccessTextSection()!.innerText!.trim());

    // Setting location permission to "Only allowed for system" should have
    // similar effect.
    setGeolocationAccessLevelPref(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertNull(getAppList());
    assertTrue(!!getNoAppHasAccessTextSection());
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noAppCanUseGeolocationText'),
        getNoAppHasAccessTextSection()!.innerText!.trim());
  });

  function initializeObserver(): Promise<void> {
    return fakeHandler.whenCalled('addObserver');
  }

  function simulateAppUpdate(app: App): void {
    fakeHandler.getObserverRemote().onAppUpdated(app);
  }

  function simulateAppRemoval(id: string): void {
    fakeHandler.getObserverRemote().onAppRemoved(id);
  }

  test('AppList displays all apps with geolocation permission', async () => {
    await initPage();
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kLocation, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kMicrophone, TriState.kAllow);
    const app3 = createApp(
        'app3_id', 'app3_name', PermissionType.kLocation, TriState.kAsk);
    const app4 = createApp(
        'app4_id', 'app4_name', PermissionType.kCamera, TriState.kBlock);


    await initializeObserver();
    simulateAppUpdate(app1);
    simulateAppUpdate(app2);
    simulateAppUpdate(app3);
    simulateAppUpdate(app4);
    await flushTasks();

    assertEquals(2, getAppList()!.items!.length);
  });

  test('Removed app are removed from appList', async () => {
    await initPage();
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kLocation, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kMicrophone, TriState.kAllow);

    await initializeObserver();
    simulateAppUpdate(app1);
    simulateAppUpdate(app2);
    await flushTasks();

    assertEquals(1, getAppList()!.items!.length);

    simulateAppRemoval(app2.id);
    await flushTasks();

    assertEquals(1, getAppList()!.items!.length);

    simulateAppRemoval(app1.id);
    await flushTasks();

    assertEquals(0, getAppList()!.items!.length);
  });

  test('AppList is alphabetically sorted', async () => {
    await initPage();
    const app1 = createApp(
        'app1_id', 'app1_name', PermissionType.kLocation, TriState.kAllow);
    const app2 = createApp(
        'app2_id', 'app2_name', PermissionType.kLocation, TriState.kAsk);
    const app3 = createApp(
        'app3_id', 'app3_name', PermissionType.kLocation, TriState.kAsk);
    const app4 = createApp(
        'app4_id', 'app4_name', PermissionType.kLocation, TriState.kBlock);


    await initializeObserver();
    simulateAppUpdate(app3);
    simulateAppUpdate(app1);
    simulateAppUpdate(app4);
    simulateAppUpdate(app2);
    await flushTasks();

    assertEquals(4, getAppList()!.items!.length);
    assertEquals(app1.name, getNthAppName(0));
    assertEquals(app2.name, getNthAppName(1));
    assertEquals(app3.name, getNthAppName(2));
    assertEquals(app4.name, getNthAppName(3));
  });

  test('Metric recorded when clicked', async () => {
    await initPage();

    assertEquals(
        0,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.DISALLOWED));
    assertEquals(
        0,
        metrics.countMetricValue(histogram(), GeolocationAccessLevel.ALLOWED));
    assertEquals(
        0,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM));

    // Default access level should be ALLOWED.
    assertEquals(GeolocationAccessLevel.ALLOWED, getGeolocationAccessLevel());

    // Change dropdown and check the corresponding metric is recorded.
    await selectGeolocationAccessLevelInDialog(
        GeolocationAccessLevel.DISALLOWED);
    assertEquals(
        1,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.DISALLOWED));

    // Change dropdown and check the corresponding metric is recorded.
    await selectGeolocationAccessLevelInDialog(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertEquals(
        1,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM));

    // Change dropdown and check the corresponding metric is recorded.
    await selectGeolocationAccessLevelInDialog(GeolocationAccessLevel.ALLOWED);
    assertEquals(
        1,
        metrics.countMetricValue(histogram(), GeolocationAccessLevel.ALLOWED));
  });

  function getManagePermissionsInChromeRow(): CrLinkRowElement|null {
    return privacyHubGeolocationSubpage.shadowRoot!
        .querySelector<CrLinkRowElement>('#managePermissionsInChromeRow');
  }

  function getNoWebsiteHasAccessTextRow(): HTMLElement|null {
    return privacyHubGeolocationSubpage.shadowRoot!.querySelector<HTMLElement>(
        '#noWebsiteHasAccessText');
  }

  test('Websites section texts', async () => {
    await initPage();
    assertEquals(
        privacyHubGeolocationSubpage.i18n('websitesSectionTitle'),
        privacyHubGeolocationSubpage.shadowRoot!
            .querySelector<HTMLElement>(
                '#websitesSectionTitle')!.innerText!.trim()),
        'problem 1';

    assertEquals(
        privacyHubGeolocationSubpage.i18n(
            'manageLocationPermissionsInChromeText'),
        getManagePermissionsInChromeRow()!.label, 'problem 2');

    // Disable geolocation access.
    setGeolocationAccessLevelPref(GeolocationAccessLevel.DISALLOWED);
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noWebsiteCanUseLocationText'),
        getNoWebsiteHasAccessTextRow()!.innerText!.trim(), 'problem 3');

    // Setting location to "only allowed for system services" should have same
    // effect as disabling.
    setGeolocationAccessLevelPref(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noWebsiteCanUseLocationText'),
        getNoWebsiteHasAccessTextRow()!.innerText!.trim(), 'problem 4');
  });

  test(
      'Websites section with external link to Chrome Settings is shown when ' +
          'location is allowed',
      async () => {
        await initPage();
        // Geolocation is set to Allowed by default.
        assertEquals(
            GeolocationAccessLevel.ALLOWED, getGeolocationAccessLevel());

        // Check that the external link is present
        assertTrue(!!getManagePermissionsInChromeRow());
        assertNull(getNoWebsiteHasAccessTextRow());
      });

  test(
      'Clicking Chrome row opens Chrome browser location permission settings',
      async () => {
        await initPage();

        assertEquals(
            PermissionType.kUnknown,
            fakeHandler.getLastOpenedBrowserPermissionSettingsType());

        getManagePermissionsInChromeRow()!.click();
        await fakeHandler.whenCalled('openBrowserPermissionSettings');

        assertEquals(
            PermissionType.kLocation,
            fakeHandler.getLastOpenedBrowserPermissionSettingsType());
      });

  test('Websites section is hidden when location is not allowed', async () => {
    await initPage();
    // Disable location access.
    setGeolocationAccessLevelPref(GeolocationAccessLevel.DISALLOWED);
    assertNull(getManagePermissionsInChromeRow());
    assertTrue(!!getNoWebsiteHasAccessTextRow());

    // Set location to "only allowed for system", UI should remain the same.
    setGeolocationAccessLevelPref(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertNull(getManagePermissionsInChromeRow());
    assertTrue(!!getNoWebsiteHasAccessTextRow());
  });

  test('System services section', async () => {
    await initPage();

    await setGeolocationAccessLevelPref(
        GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    await checkServiceSection();

    await setGeolocationAccessLevelPref(GeolocationAccessLevel.DISALLOWED);
    await checkServiceSection();

    await setGeolocationAccessLevelPref(GeolocationAccessLevel.ALLOWED);
    await checkServiceSection();
  });

  test('System services navigate to its settings page on click', async () => {
    await initPage();
    const systemServices =
        getSystemServicesFromSubpage(privacyHubGeolocationSubpage);

    // Check all 4 system services are listed.
    assertEquals(4, systemServices.length);

    // Check Timezone navigation.
    const timezone = systemServices[0]!;
    timezone.click();
    await flushTasks();
    assertEquals(
        routes.DATETIME_TIMEZONE_SUBPAGE.path,
        Router.getInstance().currentRoute.path);

    // Check NightLight navigation.
    const nightLight = systemServices[1]!;
    nightLight.click();
    await flushTasks();
    assertEquals(routes.DISPLAY.path, Router.getInstance().currentRoute.path);


    // Check DarkTheme navigation.
    const darkTheme = systemServices[2]!;
    darkTheme.click();
    assertEquals(
        loadTimeData.getString('personalizationAppUrl'),
        await openWindowProxy.whenCalled('openUrl'));
    await flushTasks();

    // Reset promise resolvers to test again on LocalWeather.
    openWindowProxy.reset();

    // Check LocalWeather navigation.
    const localWeather = systemServices[3]!;
    localWeather.click();
    assertEquals(
        loadTimeData.getString('personalizationAppUrl') +
            loadTimeData.getString('ambientSubpageRelativeUrl'),
        await openWindowProxy.whenCalled('openUrl'));
  });

  test('Timezone update in system services section', async () => {
    await initPage();
    setGeolocationAccessLevelPref(GeolocationAccessLevel.DISALLOWED);
    const systemServices =
        getSystemServicesFromSubpage(privacyHubGeolocationSubpage);

    assertEquals(4, systemServices.length);

    const timeZoneString = (tz: string) =>
        ('Blocked. Time zone is currently set to ' + tz +
         ' and can only be updated manually.');

    const sunsetScheduleString = (interval: string) =>
        'Blocked. Schedule is currently set to ' + interval +
        ' and can only be updated manually.';

    assertEquals(
        timeZoneString('Test Time Zone'),
        getSystemServicePermissionText(systemServices[0]!));
    assertEquals(
        sunsetScheduleString('7:00AM - 8:00PM'),
        getSystemServicePermissionText(systemServices[1]!));

    // Simulate timezone-changed event.
    const secondTimeZone = 'Some Other Time Zone';
    const secondSunsetSchedule = '5:00AM - 10:00PM';
    privacyHubBrowserProxy.currentTimeZoneName = secondTimeZone;
    privacyHubBrowserProxy.currentSunRiseTime = '5:00AM';
    privacyHubBrowserProxy.currentSunSetTime = '10:00PM';
    privacyHubGeolocationSubpage.notifyPath(
        'prefs.cros.system.timezone', secondTimeZone);


    // Wait for all observers to be notified.
    // This statement puts this currently executed async task at the end of the
    // JS event loop.
    await flushTasks();

    // The warning strings should now look differently.
    assertEquals(
        timeZoneString(secondTimeZone),
        getSystemServicePermissionText(systemServices[0]!));
    assertEquals(
        sunsetScheduleString(secondSunsetSchedule),
        getSystemServicePermissionText(systemServices[1]!));
  });

  test('Location controls is disabled when managed by policy', async () => {
    await initPage();
    privacyHubGeolocationSubpage.prefs.ash.user.geolocation_access_level
        .enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
    privacyHubGeolocationSubpage.prefs.ash.user.geolocation_access_level
        .controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    privacyHubGeolocationSubpage.notifyPath(
        'prefs.ash.user.geolocation_access_level.enforcement');
    privacyHubGeolocationSubpage.notifyPath(
        'prefs.ash.user.geolocation_access_level.controlledBy');
    await flushTasks();

    // Change button should be disabled.
    const changeButton =
        privacyHubGeolocationSubpage.shadowRoot!
            .querySelector<ControlledButtonElement>('#changeAccessButton')!;
    assertTrue(changeButton.disabled, 'Change button not disabled');

    // Policy indicator should be shown.
    assertTrue(
        !!changeButton.shadowRoot!.querySelector('cr-policy-pref-indicator'),
        'Policy indicator missing');
  });

  test('Location control is disabled for secondary users', async () => {
    // Simulate secondary user flow.
    loadTimeData.overrideValues({
      isSecondaryUser: true,
    });
    await initPage();

    // Change button should be disabled.
    const changeButton =
        privacyHubGeolocationSubpage.shadowRoot!
            .querySelector<ControlledButtonElement>('#changeAccessButton')!;
    assertTrue(changeButton.disabled);

    // Managed icon with tooltip should be present.
    assertTrue(!!privacyHubGeolocationSubpage.shadowRoot!
                     .querySelector<CrTooltipIconElement>('cr-tooltip-icon'));
  });
});
