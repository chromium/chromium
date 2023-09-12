// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsPowerElement} from 'chrome://os-settings/lazy_load.js';
import {Router, routes, settingMojom} from 'chrome://os-settings/os_settings.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-power>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');

  let powerSubpage: SettingsPowerElement;

  async function createSubpage(): Promise<void> {
    powerSubpage = document.createElement('settings-power');
    document.body.appendChild(powerSubpage);
    await flushTasks();
  }

  async function deepLinkToSetting(setting: settingMojom.Setting):
      Promise<void> {
    const settingId = setting.toString();
    const params = new URLSearchParams();
    params.append('settingId', settingId);
    Router.getInstance().navigateTo(routes.POWER, params);
    await flushTasks();
  }

  async function assertElementIsDeepLinked(element: HTMLElement):
      Promise<void> {
    assertTrue(isVisible(element));
    await waitAfterNextRender(element);
    assertEquals(element, powerSubpage.shadowRoot!.activeElement);
  }

  setup(async () => {
    await createSubpage();
  });

  teardown(() => {
    powerSubpage.remove();
  });

  suite('When battery status is present', () => {
    setup(async () => {
      const batteryStatus = {
        present: true,
        charging: false,
        calculating: false,
        percent: 100,
        statusText: '',
      };
      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    });

    test('Deep link to idle behavior while charging setting', async () => {
      await deepLinkToSetting(
          settingMojom.Setting.kPowerIdleBehaviorWhileCharging);

      const deepLinkElement =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>('#acIdleSelect');
      assertTrue(!!deepLinkElement);
      await assertElementIsDeepLinked(deepLinkElement);
    });

    test('Deep link to idle behavior while on battery setting', async () => {
      await deepLinkToSetting(
          settingMojom.Setting.kPowerIdleBehaviorWhileOnBattery);

      const deepLinkElement =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#batteryIdleSelect');
      assertTrue(!!deepLinkElement);
      await assertElementIsDeepLinked(deepLinkElement);
    });

    if (isRevampWayfindingEnabled) {
      test('Power idle heading is not visible', () => {
        const powerIdleLabel =
            powerSubpage.shadowRoot!.querySelector<HTMLElement>(
                '#powerIdleLabel');
        assertFalse(isVisible(powerIdleLabel));
      });
    }
  });

  suite('When battery status is not present', () => {
    setup(async () => {
      const batteryStatus = {
        present: false,
        charging: false,
        calculating: false,
        percent: 100,
        statusText: '',
      };
      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    });

    test('Deep link to idle behavior while charging setting', async () => {
      await deepLinkToSetting(
          settingMojom.Setting.kPowerIdleBehaviorWhileCharging);

      const deepLinkElement =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#noBatteryAcIdleSelect');
      assertTrue(!!deepLinkElement);
      await assertElementIsDeepLinked(deepLinkElement);
    });

    test('batteryIdleSelect is not visible', () => {
      const batteryIdleSelect =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>(
              '#batteryIdleSelect');
      assertFalse(isVisible(batteryIdleSelect));
    });

    test('acIdleSelect is not visible', () => {
      const acIdleSelect =
          powerSubpage.shadowRoot!.querySelector<HTMLElement>('#acIdleSelect');
      assertFalse(isVisible(acIdleSelect));
    });
  });
});
