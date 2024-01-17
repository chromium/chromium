// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPrivacyHubGeolocationSubpage} from 'chrome://os-settings/lazy_load.js';
import {appPermissionHandlerMojom, GeolocationAccessLevel, Router, routes, setAppPermissionProviderForTesting} from 'chrome://os-settings/os_settings.js';
import {PermissionType, TriState} from 'chrome://resources/cr_components/app_management/app_management.mojom-webui.js';
import {DomRepeat, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertNotReached, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {FakeMetricsPrivate} from '../fake_metrics_private.js';

import {FakeAppPermissionHandler} from './fake_app_permission_handler.js';
import {createApp, createFakeMetricsPrivate} from './privacy_hub_app_permission_test_util.js';

type App = appPermissionHandlerMojom.App;

suite('<settings-privacy-hub-geolocation-subpage>', () => {
  let fakeHandler: FakeAppPermissionHandler;
  let metrics: FakeMetricsPrivate;
  let privacyHubGeolocationSubpage: SettingsPrivacyHubGeolocationSubpage;

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
});
