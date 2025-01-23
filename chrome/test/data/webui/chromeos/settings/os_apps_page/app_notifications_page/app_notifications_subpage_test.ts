// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {AppNotificationsSubpage} from 'chrome://os-settings/lazy_load.js';
import type {OsSettingsRoutes, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {Router, routes, setAppNotificationProviderForTesting, setUserActionRecorderForTesting} from 'chrome://os-settings/os_settings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {FakeUserActionRecorder} from '../../fake_user_action_recorder.js';

import {FakeAppNotificationHandler} from './fake_app_notification_handler.js';


interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<settings-app-notifications-subpage>', () => {
  let page: AppNotificationsSubpage;
  let mojoApi: FakeAppNotificationHandler;
  let setQuietModeCounter = 0;
  let userActionRecorder: FakeUserActionRecorder;

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
