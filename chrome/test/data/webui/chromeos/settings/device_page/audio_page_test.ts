// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {AudioAndCaptionsPageBrowserProxyImpl} from 'chrome://os-settings/lazy_load.js';
import {CrToggleElement, Router, routes, SettingsAudioElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestAudioAndCaptionsPageBrowserProxy} from './test_audio_and_captions_page_browser_proxy.js';

suite('<settings-audio>', () => {
  let page: SettingsAudioElement;
  let browserProxy: TestAudioAndCaptionsPageBrowserProxy;

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
    // Set up test browser proxy.
    browserProxy = new TestAudioAndCaptionsPageBrowserProxy();
    AudioAndCaptionsPageBrowserProxyImpl.setInstanceForTesting(browserProxy);

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

  test(
      'Clicking on the device startup sound toggle or row updates the state',
      async () => {
        await initPage();

        const deviceStartupSoundToggle = strictQuery(
            '#deviceStartupSoundToggle', page.shadowRoot, CrToggleElement);
        assertTrue(isVisible(deviceStartupSoundToggle));
        assertFalse(deviceStartupSoundToggle.checked);

        deviceStartupSoundToggle.click();
        assertTrue(deviceStartupSoundToggle.checked);

        deviceStartupSoundToggle.click();
        assertFalse(deviceStartupSoundToggle.checked);

        const deviceStartupSoundToggleRow = strictQuery(
            '#deviceSoundsSection > .settings-box', page.shadowRoot,
            HTMLDivElement);

        // Clicking on the row should toggle the checkbox.
        deviceStartupSoundToggleRow.click();
        assertTrue(deviceStartupSoundToggle.checked);

        deviceStartupSoundToggleRow.click();
        assertFalse(deviceStartupSoundToggle.checked);
      });
});
