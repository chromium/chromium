// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrLinkRowElement, DevicePageBrowserProxyImpl, fakeKeyboards, fakeKeyboards2, PerDeviceSubsectionHeaderElement, Router, routes, SettingsPerDeviceKeyboardElement, SettingsSliderElement} from 'chrome://os-settings/os_settings.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {pressAndReleaseKeyOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

import {TestDevicePageBrowserProxy} from './test_device_page_browser_proxy.js';

suite('<settings-per-device-keyboard>', () => {
  const isRevampWayfindingEnabled =
      loadTimeData.getBoolean('isRevampWayfindingEnabled');
  let perDeviceKeyboardPage: SettingsPerDeviceKeyboardElement;
  let devicePageBrowserProxy: TestDevicePageBrowserProxy;

  function getFakeAutoRepeatPrefs() {
    return {
      settings: {
        language: {
          xkb_auto_repeat_enabled_r2: {
            key: 'prefs.settings.language.xkb_auto_repeat_enabled_r2',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
          xkb_auto_repeat_delay_r2: {
            key: 'settings.language.xkb_auto_repeat_delay_r2',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 500,
          },
          xkb_auto_repeat_interval_r2: {
            key: 'settings.language.xkb_auto_repeat_interval_r2',
            type: chrome.settingsPrivate.PrefType.NUMBER,
            value: 500,
          },
        },
      },
    };
  }

  setup(async () => {
    devicePageBrowserProxy = new TestDevicePageBrowserProxy();
    DevicePageBrowserProxyImpl.setInstanceForTesting(devicePageBrowserProxy);

    clearBody();
    perDeviceKeyboardPage =
        document.createElement('settings-per-device-keyboard');
    perDeviceKeyboardPage.set('prefs', getFakeAutoRepeatPrefs());
    perDeviceKeyboardPage.set('keyboards', fakeKeyboards);
    document.body.appendChild(perDeviceKeyboardPage);
    await flushTasks();
  });

  teardown(() => {
    Router.getInstance().resetRouteForTesting();
  });

  test('Keyboard page updates with new keyboards', async () => {
    let subsections = perDeviceKeyboardPage.shadowRoot!.querySelectorAll(
        'settings-per-device-keyboard-subsection');
    assertEquals(fakeKeyboards.length, subsections.length);
    assertFalse(subsections[0]!.get('isLastDevice'));
    assertTrue(subsections[fakeKeyboards.length - 1]!.get('isLastDevice'));

    // Check the number of subsections when the keyboard list is updated.
    perDeviceKeyboardPage.set('keyboards', fakeKeyboards2);
    await flushTasks();
    subsections = perDeviceKeyboardPage.shadowRoot!.querySelectorAll(
        'settings-per-device-keyboard-subsection');
    assertEquals(fakeKeyboards2.length, subsections.length);
    assertTrue(subsections[fakeKeyboards2.length - 1]!.get('isLastDevice'));
  });

  test(
      'Display correct name used for internal/external keyboards', async () => {
        const subsections = perDeviceKeyboardPage.shadowRoot!.querySelectorAll(
            'settings-per-device-keyboard-subsection');
        for (let i = 0; i < subsections.length; i++) {
          const subsectionHeader = strictQuery(
              'per-device-subsection-header', subsections[i]!.shadowRoot,
              PerDeviceSubsectionHeaderElement);
          const name =
              subsectionHeader.shadowRoot!.querySelector('h2')!.textContent;
          if (fakeKeyboards[i]!.isExternal) {
            assertEquals(fakeKeyboards[i]!.name, name!.trim());
          } else {
            assertTrue(subsections[i]!.i18nExists('builtInKeyboardName'));
            assertEquals('Built-in Keyboard', name!.trim());
          }
        }
      });

  test('Auto repeat toggle and slides', async () => {
    const collapse =
        perDeviceKeyboardPage.shadowRoot!.querySelector('iron-collapse');
    assert(collapse);
    assertTrue(collapse.opened);

    assertEquals(
        500,
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<SettingsSliderElement>('#delaySlider')!.pref!.value);
    assertEquals(
        500,
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<SettingsSliderElement>(
                '#repeatRateSlider')!.pref!.value);

    // Test interaction with the settings-slider's underlying cr-slider.
    pressAndReleaseKeyOn(
        perDeviceKeyboardPage.shadowRoot!.querySelector('#delaySlider')!
            .shadowRoot!.querySelector('cr-slider')!,
        37, [],
        // In the revamp, slider values and labels are reversed from low to
        // high.
        isRevampWayfindingEnabled ? 'ArrowRight' : 'ArrowLeft');
    pressAndReleaseKeyOn(
        perDeviceKeyboardPage.shadowRoot!.querySelector('#repeatRateSlider')!
            .shadowRoot!.querySelector('cr-slider')!,
        39, [], 'ArrowRight');
    await flushTasks();

    assertEquals(
        1000,
        perDeviceKeyboardPage.get(
            'prefs.settings.language.xkb_auto_repeat_delay_r2.value'));
    assertEquals(
        300,
        perDeviceKeyboardPage.get(
            'prefs.settings.language.xkb_auto_repeat_interval_r2.value'));

    // Test sliders change when prefs change.
    perDeviceKeyboardPage.set(
        'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 1500);
    await flushTasks();
    assertEquals(
        1500,
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<SettingsSliderElement>('#delaySlider')!.pref!.value);
    perDeviceKeyboardPage.set(
        'prefs.settings.language.xkb_auto_repeat_interval_r2.value', 2000);
    await flushTasks();
    assertEquals(
        2000,
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<SettingsSliderElement>(
                '#repeatRateSlider')!.pref!.value);

    // Test sliders round to nearest value when prefs change.
    perDeviceKeyboardPage.set(
        'prefs.settings.language.xkb_auto_repeat_delay_r2.value', 600);
    await flushTasks();
    assertEquals(
        500,
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<SettingsSliderElement>('#delaySlider')!.pref!.value);
    perDeviceKeyboardPage.set(
        'prefs.settings.language.xkb_auto_repeat_interval_r2.value', 45);
    await flushTasks();
    assertEquals(
        50,
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<SettingsSliderElement>(
                '#repeatRateSlider')!.pref!.value);

    perDeviceKeyboardPage.set(
        'prefs.settings.language.xkb_auto_repeat_enabled_r2.value', false);
    assertFalse(collapse.opened);
  });

  test('Open keyboard shortcut viewer', async () => {
    perDeviceKeyboardPage.shadowRoot!
        .querySelector<CrLinkRowElement>('#shortcutCustomizationApp')!.click();
    assertEquals(
        1, devicePageBrowserProxy.getCallCount('showShortcutCustomizationApp'));
  });

  test('Navigate to input tab', async () => {
    perDeviceKeyboardPage.shadowRoot!
        .querySelector<CrLinkRowElement>('#inputRow')!.click();
    assertEquals(routes.OS_LANGUAGES_INPUT, Router.getInstance().currentRoute);
  });

  test('Help message shown when no keyboards are connected', async () => {
    perDeviceKeyboardPage.set('keyboards', []);
    await flushTasks();
    assertTrue(isVisible(perDeviceKeyboardPage.shadowRoot!.querySelector(
        '#noKeyboardsConnectedContainer')));
    assertEquals(
        'No keyboard connected',
        perDeviceKeyboardPage.shadowRoot!
            .querySelector<HTMLElement>(
                '#noKeyboardsConnectedMessage')!.innerText.trim());
  });
});
