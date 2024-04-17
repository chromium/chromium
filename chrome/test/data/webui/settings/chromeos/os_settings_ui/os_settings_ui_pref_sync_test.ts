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
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('<os-settings-ui> pref sync', () => {
  let uiElement: OsSettingsUiElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(async () => {
    uiElement = document.createElement('os-settings-ui');
    document.body.appendChild(uiElement);
    await flushTasks();
    await CrSettingsPrefs.initialized;

    const queriedElement =
        uiElement.shadowRoot!.querySelector('settings-prefs');
    assertTrue(!!queriedElement);
    settingsPrefs = queriedElement;
  });

  test('Pref is updated via "user-action-setting-pref-change" event', () => {
    const prefKey = 'ash.app_notification_badging_enabled';

    // Pref value is default true.
    const defaultValue = true;
    let prefObject = settingsPrefs.get(`prefs.${prefKey}`);
    assertTrue(!!prefObject);
    assertEquals(defaultValue, prefObject.value);

    uiElement.dispatchEvent(new CustomEvent('user-action-setting-pref-change', {
      bubbles: true,
      composed: true,
      detail: {prefKey, value: !defaultValue},
    }));

    // Pref value is updated to false.
    prefObject = settingsPrefs.get(`prefs.${prefKey}`);
    assertTrue(!!prefObject);
    assertEquals(!defaultValue, prefObject.value);
  });
});
