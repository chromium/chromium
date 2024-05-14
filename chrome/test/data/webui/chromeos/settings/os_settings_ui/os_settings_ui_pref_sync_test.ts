// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Suite of tests for the overall OS Settings UI asserting pref sync behavior
 * via "user-action-setting-pref-change" event. Separated into a dedicated test
 * suite to avoid timeouts since the <os-settings-ui> element is very large.
 */

import 'chrome://os-settings/os_settings.js';

import {CrSettingsPrefs, OsSettingsUiElement, SettingsPrefsElement} from 'chrome://os-settings/os_settings.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {clearBody} from '../utils.js';

suite('<os-settings-ui> pref sync', () => {
  let uiElement: OsSettingsUiElement;
  let settingsPrefs: SettingsPrefsElement;
  let fakeSettingsPrivate: FakeSettingsPrivate;

  function getFakePrefs() {
    return [
      {
        key: 'settings.sample.setting_on_off',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'generated.resolve_timezone_by_geolocation_on_off',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
      {
        key: 'generated.resolve_timezone_by_geolocation_method_short',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 1,
      },
      {
        key: 'cros.flags.per_user_timezone_enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ];
  }

  suiteSetup(async () => {
    // Defer prefs initialization until the fake API is setup.
    CrSettingsPrefs.deferInitialization = true;
    fakeSettingsPrivate = new FakeSettingsPrivate(getFakePrefs());

    clearBody();
    uiElement = document.createElement('os-settings-ui');
    document.body.appendChild(uiElement);

    const queriedElement =
        uiElement.shadowRoot!.querySelector('settings-prefs');
    assertTrue(!!queriedElement);
    settingsPrefs = queriedElement;
    settingsPrefs.initialize(fakeSettingsPrivate);
    await flushTasks();

    // Simulate prefs initialized without waiting for the `initialized` promise.
    CrSettingsPrefs.isInitialized = true;
  });

  suiteTeardown(() => {
    CrSettingsPrefs.resetForTesting();
  });

  test(
      'Pref is updated via "user-action-setting-pref-change" event',
      async () => {
        const prefKey = 'settings.sample.setting_on_off';

        // Pref value is default true.
        const defaultValue = true;
        let prefObject = settingsPrefs.get(`prefs.${prefKey}`);
        assertTrue(!!prefObject);
        assertEquals(defaultValue, prefObject.value);

        // These events will originate from elements in `os-settings-main`.
        const mainElement =
            uiElement.shadowRoot!.querySelector('os-settings-main');
        assertTrue(!!mainElement);
        mainElement.dispatchEvent(
            new CustomEvent('user-action-setting-pref-change', {
              bubbles: true,
              composed: true,
              detail: {prefKey, value: !defaultValue},
            }));

        // Pref value is updated to false.
        prefObject = settingsPrefs.get(`prefs.${prefKey}`);
        assertTrue(!!prefObject);
        assertEquals(!defaultValue, prefObject.value);

        // Settings private API is called to commit the pref change at the OS
        // level.
        const setPrefCallArgs = await fakeSettingsPrivate.whenCalled('setPref');
        assertEquals(prefKey, setPrefCallArgs.key);
        assertEquals(!defaultValue, setPrefCallArgs.value);
      });
});
