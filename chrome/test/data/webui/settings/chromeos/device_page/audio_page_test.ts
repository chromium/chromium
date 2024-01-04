// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {Router, routes, SettingsAudioElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<settings-audio>', () => {
  let page: SettingsAudioElement;

  function getFakePrefs() {
    return {
      ash: {
        low_battery_sound: {
          enabled: {
            key: 'ash.low_battery_sound.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
        },
        charging_sounds: {
          enabled: {
            key: 'ash.charging_sounds.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: false,
          },
        },
      },
    };
  }

  async function initPage() {
    page = document.createElement('settings-audio');
    page.set('prefs', getFakePrefs());
    document.body.appendChild(page);
    await flushTasks();
  }

  setup(() => {
    Router.getInstance().navigateTo(routes.AUDIO);
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  suite('When battery status is present', () => {
    setup(async () => {
      const batteryStatus = {
        present: true,
        charging: false,
        calculating: false,
        percent: 50,
        statusText: '5 hours left',
      };
      webUIListenerCallback('battery-status-changed', batteryStatus);
      await flushTasks();
    });

    test(
        'low battery sound toggle button should reflect pref value',
        async () => {
          loadTimeData.overrideValues({
            areSystemSoundsEnabled: true,
          });
          await initPage();

          const lowBatterySoundToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#lowBatterySoundToggle');
          assert(lowBatterySoundToggle);
          assertTrue(isVisible(lowBatterySoundToggle));

          assertTrue(lowBatterySoundToggle.checked);
          assertTrue(page.prefs.ash.low_battery_sound.enabled.value);

          lowBatterySoundToggle.click();
          assertFalse(lowBatterySoundToggle.checked);
          assertFalse(page.prefs.ash.low_battery_sound.enabled.value);
        });

    test(
        'charging sounds toggle button should reflect pref value', async () => {
          loadTimeData.overrideValues({
            areSystemSoundsEnabled: true,
          });
          await initPage();

          const chargingSoundsToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#chargingSoundsToggle');
          assert(chargingSoundsToggle);
          assertTrue(isVisible(chargingSoundsToggle));

          assertFalse(chargingSoundsToggle.checked);
          assertFalse(page.prefs.ash.charging_sounds.enabled.value);

          chargingSoundsToggle.click();
          assertTrue(chargingSoundsToggle.checked);
          assertTrue(page.prefs.ash.charging_sounds.enabled.value);
        });
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

    test(
        'charging sounds toggle and low battery sound are not visible',
        async () => {
          const chargingSoundsToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#chargingSoundsToggle');
          assertFalse(isVisible(chargingSoundsToggle));

          const lowBatterySoundToggle =
              page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                  '#lowBatterySoundToggle');
          assertFalse(isVisible(lowBatterySoundToggle));
        });
  });
});
