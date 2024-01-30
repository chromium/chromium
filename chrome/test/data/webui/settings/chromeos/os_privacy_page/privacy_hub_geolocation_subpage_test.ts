// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {PrivacyHubBrowserProxyImpl, SettingsPrivacyHubGeolocationSubpage} from 'chrome://os-settings/lazy_load.js';
import {appPermissionHandlerMojom, CrLinkRowElement, GeolocationAccessLevel, Router, routes, setAppPermissionProviderForTesting, SettingsPrivacyHubSystemServiceRow} from 'chrome://os-settings/os_settings.js';
import {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotReached, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

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
      },
    };
    privacyHubGeolocationSubpage.prefs = prefs;
    document.body.appendChild(privacyHubGeolocationSubpage);
    flush();
  }


  setup(() => {
    fakeHandler = new FakeAppPermissionHandler();
    setAppPermissionProviderForTesting(fakeHandler);
    metrics = createFakeMetricsPrivate();
    privacyHubBrowserProxy = new TestPrivacyHubBrowserProxy();
    PrivacyHubBrowserProxyImpl.setInstanceForTesting(privacyHubBrowserProxy);

    Router.getInstance().navigateTo(routes.PRIVACY_HUB_GEOLOCATION);
  });

  teardown(() => {
    privacyHubGeolocationSubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  function histogram(): string {
    return 'ChromeOS.PrivacyHub.Geolocation.AccessLevelChanged.SystemSettings';
  }

  function setGeolocationAccessLevel(accessLevel: GeolocationAccessLevel) {
    const dropdown = privacyHubGeolocationSubpage.shadowRoot!
                         .querySelector('settings-dropdown-menu')!.shadowRoot!
                         .querySelector<HTMLSelectElement>('#dropdownMenu')!;
    assertTrue(!!dropdown);

    dropdown.value = accessLevel.toString();
    dropdown.dispatchEvent(new CustomEvent('change'));
    flush();
  }

  function getGeolocationAccessLevel(): GeolocationAccessLevel {
    const dropdown = privacyHubGeolocationSubpage.shadowRoot!
                         .querySelector('settings-dropdown-menu')!.shadowRoot!
                         .querySelector<HTMLSelectElement>('#dropdownMenu')!;
    assertTrue(!!dropdown);

    switch (dropdown.value) {
      case '0':
        return GeolocationAccessLevel.DISALLOWED;
      case '1':
        return GeolocationAccessLevel.ALLOWED;
      case '2':
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

  function checkService(
      systemService: SettingsPrivacyHubSystemServiceRow, nameVarName: string,
      expectedName: string, allowedTextVarName: string, allowedText: string,
      blockedText: string) {
    // Check  service name.
    assertEquals(expectedName, privacyHubGeolocationSubpage.i18n(nameVarName));
    assertEquals(expectedName, getSystemServiceName(systemService));

    // Check subtext.
    switch (getGeolocationAccessLevel()) {
      case GeolocationAccessLevel.DISALLOWED:
        assertEquals(
            blockedText, getSystemServicePermissionText(systemService));
        break;
      case GeolocationAccessLevel.ALLOWED:
        // Falls through to ONLY_ALLOWED_FOR_SYSTEM
      case GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM:
        assertEquals(
            allowedText, privacyHubGeolocationSubpage.i18n(allowedTextVarName));
        assertEquals(
            allowedText, getSystemServicePermissionText(systemService));
        break;
    }
  }

  function checkServiceSection() {
    assertEquals(
        privacyHubGeolocationSubpage.i18n(
            'privacyHubSystemServicesSectionTitle'),
        privacyHubGeolocationSubpage.shadowRoot!
            .querySelector<HTMLElement>(
                '#systemServicesSectionTitle')!.innerText!.trim());

    const systemServices =
        getSystemServicesFromSubpage(privacyHubGeolocationSubpage);

    assertEquals(4, systemServices.length);
    checkService(
        systemServices[0]!, 'privacyHubSystemServicesAutomaticTimeZoneName',
        'Automatic time zone', 'privacyHubSystemServicesAllowedText', 'Allowed',
        'Blocked. Time zone is currently set to ' +
            'Test Time Zone' +
            ' and can only be updated manually.');
    checkService(
        systemServices[1]!, 'privacyHubSystemServicesSunsetScheduleName',
        'Sunset schedule', 'privacyHubSystemServicesAllowedText', 'Allowed',
        'Blocked. Schedule is currently set to 7:00AM - 8:00PM' +
            ' and can only be updated manually.');
    checkService(
        systemServices[2]!, 'privacyHubSystemServicesLocalWeatherName',
        'Local weather', 'privacyHubSystemServicesAllowedText', 'Allowed',
        'Blocked');
    checkService(
        systemServices[3]!, 'privacyHubSystemServicesDarkThemeName',
        'Dark theme', 'privacyHubSystemServicesAllowedText', 'Allowed',
        'Blocked');
  }

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
    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);

    assertNull(getAppList());
    assertTrue(!!getNoAppHasAccessTextSection());
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noAppCanUseGeolocationText'),
        getNoAppHasAccessTextSection()!.innerText!.trim());

    // Setting location permission to "Only allowed for system" should have
    // similar effect.
    setGeolocationAccessLevel(GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
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
    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);
    assertEquals(
        1,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.DISALLOWED));

    // Change dropdown and check the corresponding metric is recorded.
    setGeolocationAccessLevel(GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertEquals(
        1,
        metrics.countMetricValue(
            histogram(), GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM));

    // Change dropdown and check the corresponding metric is recorded.
    setGeolocationAccessLevel(GeolocationAccessLevel.ALLOWED);
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
                '#websitesSectionTitle')!.innerText!.trim());

    assertEquals(
        privacyHubGeolocationSubpage.i18n(
            'manageLocationPermissionsInChromeText'),
        getManagePermissionsInChromeRow()!.label);

    // Disable geolocation access.
    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noWebsiteCanUseLocationText'),
        getNoWebsiteHasAccessTextRow()!.innerText!.trim());

    // Setting location to "only allowed for system services" should have same
    // effect as disabling.
    setGeolocationAccessLevel(GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertEquals(
        privacyHubGeolocationSubpage.i18n('noWebsiteCanUseLocationText'),
        getNoWebsiteHasAccessTextRow()!.innerText!.trim());
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
    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);
    assertNull(getManagePermissionsInChromeRow());
    assertTrue(!!getNoWebsiteHasAccessTextRow());

    // Set location to "only allowed for system", UI should remain the same.
    setGeolocationAccessLevel(GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    assertNull(getManagePermissionsInChromeRow());
    assertTrue(!!getNoWebsiteHasAccessTextRow());
  });

  test('System services section', async () => {
    await initPage();

    setGeolocationAccessLevel(GeolocationAccessLevel.ONLY_ALLOWED_FOR_SYSTEM);
    checkServiceSection();

    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);
    checkServiceSection();

    setGeolocationAccessLevel(GeolocationAccessLevel.ALLOWED);
    checkServiceSection();
  });

  test('Timezone update in system services section', async () => {
    await initPage();
    setGeolocationAccessLevel(GeolocationAccessLevel.DISALLOWED);
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

});
