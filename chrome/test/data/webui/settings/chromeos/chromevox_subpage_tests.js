// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the ChromeVox subpage in ChromeOS Settings.
 */

import 'chrome://os-settings/chromeos/lazy_load.js';

import {ChromeVoxSubpageBrowserProxyImpl, CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {addWebUiListener, webUIListenerCallback} from 'chrome://resources/js/cr.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertDeepEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestChromeVoxSubpageBrowserProxy} from './test_chromevox_subpage_browser_proxy.js';

/**
 * Control types for pref-based settings.
 * @enum {string}
 */
export const ControlType = {
  DROPDOWN: 'dropdown',
  TOGGLE: 'toggle',
};

suite('ChromeVoxSubpageTests', function() {
  /** @type {SettingsChromeVoxSubpageElement} */
  let page = null;
  let browserProxy = null;

  setup(async function() {
    browserProxy = new TestChromeVoxSubpageBrowserProxy();
    ChromeVoxSubpageBrowserProxyImpl.setInstanceForTesting(browserProxy);

    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-chromevox-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(function() {
    page.remove();
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
      id: 'brailleWordWrapToggle',
      prefKey: 'settings.a11y.chromevox.braille_word_wrap',
      defaultValue: true,
      secondaryValue: false,
      type: ControlType.TOGGLE,
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
  ];

  settingsControls.forEach(control => {
    const {id, prefKey, defaultValue, secondaryValue, type} = control;

    test(`ChromeVox ${type} ${id} syncs to Pref: ${prefKey}`, async () => {
      // Make sure control exists.
      const control = page.shadowRoot.querySelector(`#${id}`);
      assertTrue(!!control);

      // Make sure pref is set to the default value.
      let pref = page.getPref(prefKey);
      assertEquals(defaultValue, pref.value);

      // Update control to secondary value.
      switch (type) {
        case ControlType.TOGGLE:
          control.click();
          break;
        case ControlType.DROPDOWN:
          await waitAfterNextRender(control);
          const controlElement = control.shadowRoot.querySelector('select');
          controlElement.value = secondaryValue;
          controlElement.dispatchEvent(
              new CustomEvent('change', {bubbles: true, composed: true}));
          break;
      }

      // Make sure pref is set to secondary value.
      pref = page.getPref(prefKey);
      assertEquals(secondaryValue, pref.value);
    });
  });

  test('event stream filter toggles sync to prefs', async () => {
    // Enable event stream logging to allow enabling filter toggles.
    const loggingToggle =
        page.shadowRoot.querySelector('#enableEventStreamLoggingToggle');
    loggingToggle.click();
    await waitAfterNextRender(loggingToggle);

    // Get all event stream filter prefs.
    let pref = page.getPref('settings.a11y.chromevox.event_stream_filters');

    // Toggle each filter, verify each pref is set.
    page.eventStreamFilters_.forEach(filter => {
      const toggle = page.shadowRoot.querySelector('#' + filter);

      // Make sure toggle exists.
      assertTrue(!!toggle);

      // Make sure pref filter state is false or undefined (key is not present).
      assertTrue([false, undefined].includes(pref.value[filter]));

      // Enable event stream filter toggle.
      toggle.click();

      // Make sure event stream filter pref state is true.
      pref = page.getPref('settings.a11y.chromevox.event_stream_filters');
      assertTrue(pref.value[filter]);
    });
  });

  test('voices are ordered', async function() {
    // Make sure voices are ordered with the system default voice first, then
    // Google voices, then eSpeak, then local, then remote.
    const voiceDropdown = page.shadowRoot.querySelector('#voiceDropdown');
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
});
