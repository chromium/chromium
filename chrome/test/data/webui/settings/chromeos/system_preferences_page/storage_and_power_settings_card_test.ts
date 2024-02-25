// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {OsSettingsRoutes, Router, routes, StorageAndPowerSettingsCardElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

interface SubpageTriggerData {
  triggerSelector: string;
  routeName: keyof OsSettingsRoutes;
}

suite('<storage-and-power-settings-card>', () => {
  let storageAndPowerSettingsCard: StorageAndPowerSettingsCardElement;

  function createCardElement(): void {
    storageAndPowerSettingsCard =
        document.createElement('storage-and-power-settings-card');
    document.body.appendChild(storageAndPowerSettingsCard);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({isDemoSession: false});
  });

  teardown(() => {
    storageAndPowerSettingsCard.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Storage row is visible', () => {
    createCardElement();
    const storageRow =
        storageAndPowerSettingsCard.shadowRoot!.querySelector('#storageRow');
    assertTrue(isVisible(storageRow));
  });

  test('Power row is visible', () => {
    createCardElement();
    const powerRow =
        storageAndPowerSettingsCard.shadowRoot!.querySelector('#powerRow');
    assertTrue(isVisible(powerRow));
  });

  suite('For a demo session', () => {
    setup(() => {
      loadTimeData.overrideValues({isDemoSession: true});
    });

    test('Storage row is not visible', () => {
      createCardElement();
      const storageRow =
          storageAndPowerSettingsCard.shadowRoot!.querySelector('#storageRow');
      assertFalse(isVisible(storageRow));
    });

    test('Power row is visible', () => {
      createCardElement();
      const powerRow =
          storageAndPowerSettingsCard.shadowRoot!.querySelector('#powerRow');
      assertTrue(isVisible(powerRow));
    });
  });

  const subpageTriggerData: SubpageTriggerData[] = [
    {
      triggerSelector: '#storageRow',
      routeName: 'STORAGE',
    },
    {
      triggerSelector: '#powerRow',
      routeName: 'POWER',
    },
  ];
  subpageTriggerData.forEach(({triggerSelector, routeName}) => {
    test(
        `Row for ${routeName} is focused when returning from subpage`,
        async () => {
          Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES);
          createCardElement();

          const subpageTrigger =
              storageAndPowerSettingsCard.shadowRoot!
                  .querySelector<HTMLElement>(triggerSelector);
          assertTrue(!!subpageTrigger);

          // Subpage trigger navigates to subpage for route
          subpageTrigger.click();
          assertEquals(routes[routeName], Router.getInstance().currentRoute);

          // Navigate back
          const popStateEventPromise = eventToPromise('popstate', window);
          Router.getInstance().navigateToPreviousRoute();
          await popStateEventPromise;
          await waitAfterNextRender(storageAndPowerSettingsCard);

          assertEquals(
              subpageTrigger,
              storageAndPowerSettingsCard.shadowRoot!.activeElement,
              `${triggerSelector} should be focused.`);
        });
  });
});
