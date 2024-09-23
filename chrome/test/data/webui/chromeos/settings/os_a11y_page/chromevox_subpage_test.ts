// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the ChromeVox subpage in ChromeOS Settings.
 */

import 'chrome://os-settings/lazy_load.js';

import {SettingsChromeVoxSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {ChromeVoxSubpageBrowserProxyImpl, CrSettingsPrefs, SettingsDropdownMenuElement, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestChromeVoxSubpageBrowserProxy} from './test_chromevox_subpage_browser_proxy.js';

/**
 * Control types for pref-based settings.
 */
const enum ControlType {
  DROPDOWN = 'dropdown',
  TOGGLE = 'toggle',
  INPUT = 'input',
}

suite('<settings-chromevox-subpage>', () => {
  let page: SettingsChromeVoxSubpageElement;
  let browserProxy: TestChromeVoxSubpageBrowserProxy;
  let prefElement: SettingsPrefsElement;

  setup(async () => {
    browserProxy = new TestChromeVoxSubpageBrowserProxy();
    ChromeVoxSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-chromevox-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
  });

  const settingsControls = [
    {
      id: 'voiceDropdown',
      prefKey: 'settings.a11y.chromevox.voice_name',
      defaultValue: 'chromeos_system_voice',
      secondaryValue: 'Chrome OS US English',
      type: ControlType.DROPDOWN,
    },
    {
      id: 'languageSwitchingToggle',
      prefKey: 'settings.a11y.chromevox.language_switching',
      defaultValue: false,
      secondaryValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'enableSpeechLoggingToggle',
      prefKey: 'settings.a11y.chromevox.enable_speech_logging',
      defaultValue: false,
      secondaryValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'brailleTableTypeDropdown',
      prefKey: 'settings.a11y.chromevox.braille_table_type',
      defaultValue: 'brailleTable8',
      secondaryValue: 'brailleTable6',
      type: ControlType.DROPDOWN,
    },
    {
      id: 'brailleWordWrapToggle',
      prefKey: 'settings.a11y.chromevox.braille_word_wrap',
      defaultValue: true,
      secondaryValue: false,
      type: ControlType.TOGGLE,
    },
    {
      id: 'virtualBrailleDisplayRowsInput',
      prefKey: 'settings.a11y.chromevox.virtual_braille_rows',
      defaultValue: 1,
      secondaryValue: 2,
      correctedValues: [
        ['', 1],
        [0, 1],
        [1, 1],
        [2, 2],
        [98, 98],
        [99, 99],
        [100, 99],
        [-4, 99],
        [-999, 99],
        [20, 20],
        ['', 20],
      ],
      type: ControlType.INPUT,
    },
    {
      id: 'virtualBrailleDisplayColumnsInput',
      prefKey: 'settings.a11y.chromevox.virtual_braille_columns',
      defaultValue: 40,
      secondaryValue: 80,
      correctedValues: [
        ['', 40],
        [0, 40],
        [1, 1],
        [2, 2],
        [98, 98],
        [99, 99],
        [100, 99],
        [-4, 99],
        [-999, 99],
        [20, 20],
        ['', 20],
      ],
      type: ControlType.INPUT,
    },
    {
      id: 'menuBrailleCommandsToggle',
      prefKey: 'settings.a11y.chromevox.menu_braille_commands',
      defaultValue: false,
      secondaryValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'enableEarconLoggingToggle',
      prefKey: 'settings.a11y.chromevox.enable_earcon_logging',
      defaultValue: false,
      secondaryValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'enableBrailleLoggingToggle',
      prefKey: 'settings.a11y.chromevox.enable_braille_logging',
      defaultValue: false,
      secondaryValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'enableEventStreamLoggingToggle',
      prefKey: 'settings.a11y.chromevox.enable_event_stream_logging',
      defaultValue: false,
      secondaryValue: true,
      type: ControlType.TOGGLE,
    },
    {
      id: 'virtualBrailleDisplayStyleDropdown',
      prefKey: 'settings.a11y.chromevox.braille_side_by_side',
      defaultValue: true,
      secondaryValue: false,
      type: ControlType.DROPDOWN,
    },
  ];

  settingsControls.forEach(control => {
    const {id, prefKey, defaultValue, secondaryValue, type, correctedValues} =
        control;

    test(`ChromeVox ${type} ${id} syncs to Pref: ${prefKey}`, async () => {
      // Make sure control exists.
      const control = page.shadowRoot!.querySelector<HTMLElement>(`#${id}`);
      assert(control);

      // Make sure pref is set to the default value.
      let pref = page.getPref(prefKey);
      assertEquals(defaultValue, pref.value);

      switch (type) {
        case ControlType.TOGGLE:
          // Make sure toggle control is set to the default value.
          await waitAfterNextRender(control);
          const crToggleElement =
              control.shadowRoot!.querySelector('cr-toggle');
          assertEquals(defaultValue, crToggleElement!.checked);
          // Click toggle control to attempt updating to secondary value.
          control.click();
          break;

        case ControlType.DROPDOWN:
          // Make sure dropdown is set to the default value.
          await waitAfterNextRender(control);
          const selectElement = control.shadowRoot!.querySelector('select');
          assert(selectElement);
          assertEquals(String(defaultValue), selectElement.value);
          // Update dropdown to secondary value.
          selectElement.value = String(secondaryValue);
          selectElement.dispatchEvent(
              new CustomEvent('change', {bubbles: true, composed: true}));
          break;

        case ControlType.INPUT:
          // Make sure input is set to the default value.
          await waitAfterNextRender(control);
          const inputElement = control.shadowRoot!.querySelector('input');
          assert(inputElement);
          assertEquals(String(defaultValue), inputElement.value);
          // Make sure out-of-range values get updated to correct values.
          assert(correctedValues);
          for (const [startValue, correctedValue] of correctedValues) {
            inputElement.value = String(startValue);
            inputElement.dispatchEvent(
                new CustomEvent('input', {bubbles: true, composed: true}));
            inputElement.dispatchEvent(
                new CustomEvent('focusout', {bubbles: true, composed: true}));
            await waitAfterNextRender(control);
            assertEquals(String(correctedValue), inputElement.value);
          }
          // Update input to secondary value.
          inputElement.value = String(secondaryValue);
          inputElement.dispatchEvent(
              new CustomEvent('input', {bubbles: true, composed: true}));
          break;
      }

      // Make sure pref is set to secondary value.
      pref = page.getPref(prefKey);
      assertEquals(secondaryValue, pref.value);
    });
  });

  test('event stream filter toggles sync to prefs', async () => {
    // Enable event stream logging to allow enabling filter toggles.
    const loggingToggle = page.shadowRoot!.querySelector<HTMLElement>(
        '#enableEventStreamLoggingToggle');
    assert(loggingToggle);
    loggingToggle.click();
    await waitAfterNextRender(loggingToggle);

    // Get all event stream filter prefs.
    let pref = page.getPref('settings.a11y.chromevox.event_stream_filters');

    // Toggle each filter, verify each pref is set.
    page.get('eventStreamFilters_').forEach((filter: string) => {
      const toggle = page.shadowRoot!.querySelector<HTMLElement>('#' + filter);

      // Make sure toggle exists.
      assert(toggle);

      // Make sure pref filter state is false or undefined (key is not present).
      assertTrue([false, undefined].includes(pref.value[filter]));

      // Enable event stream filter toggle.
      toggle.click();

      // Make sure event stream filter pref state is true.
      pref = page.getPref('settings.a11y.chromevox.event_stream_filters');
      assertTrue(pref.value[filter]);
    });
  });

  test('voices are ordered', () => {
    // Make sure voices are ordered with the system default voice first, then
    // Google voices, then eSpeak, then local, then remote.
    const voiceDropdown =
        page.shadowRoot!.querySelector<SettingsDropdownMenuElement>(
            '#voiceDropdown');
    assert(voiceDropdown);
    const expectedMenuOptions = [
      {name: 'System Text-to-Speech voice', value: 'chromeos_system_voice'},
      {name: 'Chrome OS US English', value: 'Chrome OS US English'},
      {name: 'Chrome OS हिन्दी', value: 'Chrome OS हिन्दी'},
      {name: 'eSpeak Turkish', value: 'eSpeak Turkish'},
      {name: 'default-coolnet', value: 'default-coolnet'},
      {name: 'bnm', value: 'bnm'},
      {name: 'bnx', value: 'bnx'},
    ];
    assertDeepEquals(expectedMenuOptions, voiceDropdown.menuOptions);
  });

  test('connect button works', async function() {
    // Mock chrome.bluetooth.getDevice using `display` as the backing source.
    const displays = [{name: 'VarioUltra', address: 'abcd1234', paired: true}];
    chrome.bluetooth.getDevice = async address =>
        displays.find(display => display.address === address)!;

    // Get Bluetooth Braille Display UI element.
    const bluetoothBrailleDisplayUi =
        page.shadowRoot!.querySelector('bluetooth-braille-display-ui');
    assert(bluetoothBrailleDisplayUi);

    // Update list of devices.
    await bluetoothBrailleDisplayUi.onDisplayListChanged(displays);

    // Get Bluetooth Braille Display dropdown element.
    const dropdownMenu =
        bluetoothBrailleDisplayUi.shadowRoot!
            .querySelector<SettingsDropdownMenuElement>('#displaySelect');
    assert(dropdownMenu);

    // Get Bluetooth Braille Display select element.
    const selectElement = dropdownMenu.shadowRoot!.querySelector('select');
    assert(selectElement);

    // Select VarioUltra simulated device.
    selectElement.value = 'abcd1234';
    selectElement.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    await waitAfterNextRender(selectElement);

    // Update list of devices.
    await bluetoothBrailleDisplayUi.onDisplayListChanged(displays);
  });

  test('no custom dropdown item shown', async function() {
    // Get Bluetooth Braille Display UI element.
    const bluetoothBrailleDisplayUi =
        page.shadowRoot!.querySelector('bluetooth-braille-display-ui');
    assert(bluetoothBrailleDisplayUi);

    // Get Bluetooth Braille Display dropdown element.
    const dropdownMenu =
        bluetoothBrailleDisplayUi.shadowRoot!
            .querySelector<SettingsDropdownMenuElement>('#displaySelect');
    assert(dropdownMenu);

    // Get Bluetooth Braille Display select element.
    const selectElement = dropdownMenu.shadowRoot!.querySelector('select');
    assert(selectElement);

    // Verify Bluetooth Braille Display select element is blank (not custom).
    assertEquals('', selectElement.value);
  });

  test('braille display shown', async function() {
    // Mock chrome.bluetooth.getDevice using `display` as the backing source.
    const displays = [{name: 'VarioUltra', address: 'abcd1234', paired: true}];
    chrome.bluetooth.getDevice = async address =>
        displays.find(display => display.address === address)!;

    // Get Bluetooth Braille Display UI element.
    const bluetoothBrailleDisplayUi =
        page.shadowRoot!.querySelector('bluetooth-braille-display-ui');
    assert(bluetoothBrailleDisplayUi);

    // Update list of devices.
    await bluetoothBrailleDisplayUi.onDisplayListChanged(displays);

    // Get Bluetooth Braille Display dropdown element.
    const dropdownMenu =
        bluetoothBrailleDisplayUi.shadowRoot!
            .querySelector<SettingsDropdownMenuElement>('#displaySelect');
    assert(dropdownMenu);

    // Get Bluetooth Braille Display select element.
    const selectElement = dropdownMenu.shadowRoot!.querySelector('select');
    assert(selectElement);

    // Verify VarioUltra Bluetooth Braille Display is selected.
    assertEquals('abcd1234', selectElement.value);
  });
});
