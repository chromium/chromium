// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsKeyboardAndTextInputPageElement} from 'chrome://os-settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsSliderElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

// TODO(https://issuetracker.google.com/378556940): Merge tests with
// keyboard_and_text_input_page_test.ts once feature is enabled by default.
suite('<filter-keys>', () => {
  let page: SettingsKeyboardAndTextInputPageElement;
  let prefElement: SettingsPrefsElement;

  const slowKeysToggleId = 'slowKeysToggle';
  const slowKeysDelaySliderId = 'slowKeysDelaySlider';
  const bounceKeysToggleId = 'bounceKeysToggle';
  const bounceKeysDelaySliderId = 'bounceKeysDelaySlider';

  const millisInSec = 1000;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);
    await CrSettingsPrefs.initialized;

    page = document.createElement('settings-keyboard-and-text-input-page');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    loadTimeData.overrideValues({
      isAccessibilityBounceKeysEnabled: true,
      isAccessibilitySlowKeysEnabled: true,
    });
    Router.getInstance().navigateTo(routes.A11Y_KEYBOARD_AND_TEXT_INPUT);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'bounce keys and slow keys features disabled shows no rows', async () => {
        loadTimeData.overrideValues({
          isAccessibilityBounceKeysEnabled: false,
          isAccessibilitySlowKeysEnabled: false,
        });
        await initPage();

        assertFalse(
            !!page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                `#${slowKeysToggleId}`));
        assertFalse(!!page.shadowRoot!.querySelector<SettingsSliderElement>(
            `#${slowKeysDelaySliderId}`));
        assertFalse(
            !!page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                `#${bounceKeysToggleId}`));
        assertFalse(!!page.shadowRoot!.querySelector<SettingsSliderElement>(
            `#${bounceKeysDelaySliderId}`));
      });

  test('bounce keys feature disabled shows no bounce keys rows', async () => {
    loadTimeData.overrideValues({
      isAccessibilityBounceKeysEnabled: false,
    });
    await initPage();

    assertTrue(!!page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        `#${slowKeysToggleId}`));
    assertTrue(!!page.shadowRoot!.querySelector<SettingsSliderElement>(
        `#${slowKeysDelaySliderId}`));
    assertFalse(!!page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        `#${bounceKeysToggleId}`));
    assertFalse(!!page.shadowRoot!.querySelector<SettingsSliderElement>(
        `#${bounceKeysDelaySliderId}`));
  });

  test('slow keys feature disabled shows no slow keys rows', async () => {
    loadTimeData.overrideValues({
      isAccessibilitySlowKeysEnabled: false,
    });
    await initPage();

    assertFalse(!!page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        `#${slowKeysToggleId}`));
    assertFalse(!!page.shadowRoot!.querySelector<SettingsSliderElement>(
        `#${slowKeysDelaySliderId}`));
    assertTrue(!!page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        `#${bounceKeysToggleId}`));
    assertTrue(!!page.shadowRoot!.querySelector<SettingsSliderElement>(
        `#${bounceKeysDelaySliderId}`));
  });

  const filterKeysRows = [
    {
      toggleId: slowKeysToggleId,
      sliderId: slowKeysDelaySliderId,
      enabledPrefKey: 'settings.a11y.slow_keys_enabled',
      delayPrefKey: 'settings.a11y.slow_keys_delay_ms',
    },
    {
      toggleId: bounceKeysToggleId,
      sliderId: bounceKeysDelaySliderId,
      enabledPrefKey: 'settings.a11y.bounce_keys_enabled',
      delayPrefKey: 'settings.a11y.bounce_keys_delay_ms',
    },
  ];

  filterKeysRows.forEach(
      ({toggleId, sliderId, enabledPrefKey, delayPrefKey}) => {
        test(
            `clicking ${toggleId} toggle toggles ${sliderId} slider`,
            async () => {
              await initPage();

              const toggle =
                  page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                      `#${toggleId}`);
              assertTrue(!!toggle);
              assertTrue(isVisible(toggle));
              assertFalse(toggle.checked);
              const enabledPref = page.getPref(enabledPrefKey);
              assertFalse(enabledPref.value);

              const slider =
                  page.shadowRoot!.querySelector<SettingsSliderElement>(
                      `#${sliderId}`);
              assertTrue(!!slider);
              assertFalse(isVisible(slider));

              toggle.click();
              assertTrue(toggle.checked);
              assertTrue(enabledPref.value);
              assertTrue(isVisible(slider));
              const delayPref = page.getPref(delayPrefKey);
              assertEquals(slider.pref.value * millisInSec, delayPref.value);

              toggle.click();
              assertFalse(toggle.checked);
              assertFalse(enabledPref.value);
              assertFalse(isVisible(slider));
            });
      });

  filterKeysRows.forEach(
      ({toggleId, sliderId, enabledPrefKey, delayPrefKey}) => {
        test(
            `changing ${sliderId} slider updates ${delayPrefKey} pref`,
            async () => {
              await initPage();

              // Enable pref to make slider show up.
              page.setPrefValue(enabledPrefKey, true);
              page.setPrefValue(delayPrefKey, 300);

              const toggle =
                  page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
                      `#${toggleId}`);
              assertTrue(!!toggle);
              assertTrue(isVisible(toggle));
              assertTrue(toggle.checked);

              const slider =
                  page.shadowRoot!.querySelector<SettingsSliderElement>(
                      `#${sliderId}`);
              assertTrue(!!slider);
              assertTrue(isVisible(slider));
              assertEquals(slider.pref.value, 0.3);

              const crSlider = slider.shadowRoot!.querySelector('cr-slider');
              assertTrue(!!crSlider);

              // Press left arrow to make delay shorter.
              pressAndReleaseKeyOn(crSlider, 37, [], 'ArrowLeft');
              await flush();

              assertEquals(slider.pref.value, 0.2);
              const delayPref = page.getPref(delayPrefKey);
              assertEquals(delayPref.value, 200);
            });
      });

  [{
    toggleId: slowKeysToggleId,
    settingId: 1555,
  },
   {
     toggleId: bounceKeysToggleId,
     settingId: 1554,
   },
  ].forEach(({toggleId, settingId}) => {
    test(`deep link to ${toggleId} toggle`, async () => {
      await initPage();

      const params = new URLSearchParams();
      params.append('settingId', `${settingId}`);
      Router.getInstance().navigateTo(
          routes.A11Y_KEYBOARD_AND_TEXT_INPUT, params);
      flush();

      const deepLinkElement = page.shadowRoot!.querySelector(`#${
          toggleId}`)!.shadowRoot!.querySelector('cr-toggle');
      assertTrue(!!deepLinkElement);
      await waitAfterNextRender(deepLinkElement);
      assertEquals(getDeepActiveElement(), deepLinkElement);
    });
  });
});
