// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsGlicDataPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, SettingsGlicDataPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('GlicDataPage', function() {
  let page: SettingsGlicDataPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-glic-data-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('GeolocationToggleEnabled', () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, true);

    assertTrue(page.$.geolocationToggle.checked);
  });

  test('GeolocationToggleDisabled', () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

    assertFalse(page.$.geolocationToggle.checked);
  });

  test('GeolocationToggleChange', () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

    const geolocationToggle = page.$.geolocationToggle;
    assertTrue(!!geolocationToggle);

    geolocationToggle.click();
    assertEquals(true, page.getPref(PrefName.GEOLOCATION_ENABLED).value);
    assertTrue(geolocationToggle.checked);

    geolocationToggle.click();
    assertEquals(false, page.getPref(PrefName.GEOLOCATION_ENABLED).value);
    assertFalse(geolocationToggle.checked);
  });

  test('MicrophoneToggleEnabled', () => {
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, true);

    assertTrue(page.$.microphoneToggle.checked);
  });

  test('MicrophoneToggleDisabled', () => {
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

    assertFalse(page.$.microphoneToggle.checked);
  });

  test('MicrophoneToggleChange', () => {
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

    const microphoneToggle = page.$.microphoneToggle;
    assertTrue(!!microphoneToggle);

    microphoneToggle.click();
    assertEquals(true, page.getPref(PrefName.MICROPHONE_ENABLED).value);
    assertTrue(microphoneToggle.checked);

    microphoneToggle.click();
    assertEquals(false, page.getPref(PrefName.MICROPHONE_ENABLED).value);
    assertFalse(microphoneToggle.checked);
  });

  test('TabContextToggleEnabled', () => {
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, true);

    assertTrue(page.$.tabAccessToggle.checked);
  });

  test('TabContextToggleDisabled', () => {
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

    assertFalse(page.$.tabAccessToggle.checked);
  });

  test('TabContextToggleChange', () => {
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

    const tabAccessToggle = page.$.tabAccessToggle;
    assertTrue(!!tabAccessToggle);

    tabAccessToggle.click();
    assertEquals(true, page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertTrue(tabAccessToggle.checked);

    tabAccessToggle.click();
    assertEquals(false, page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertFalse(tabAccessToggle.checked);
  });
});
