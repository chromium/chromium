// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrCollapseElement} from 'chrome://settings/lazy_load.js';
import type {SettingsGlicDataPageElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, SettingsGlicDataPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

const POLICY_ENABLED_VALUE = 0;
const POLICY_DISABLED_VALUE = 1;

suite('GlicDataPage', function() {
  let page: SettingsGlicDataPageElement;
  let settingsPrefs: SettingsPrefsElement;

  function $<T extends HTMLElement = HTMLElement>(id: string): T|null {
    return page.shadowRoot!.querySelector<T>(`#${id}`);
  }

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-glic-data-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);

    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);
    return flushTasks();
  });

  test('GeolocationToggleEnabled', () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, true);

    assertTrue($<SettingsToggleButtonElement>('geolocationToggle')!.checked);
  });

  test('GeolocationToggleDisabled', () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

    assertFalse($<SettingsToggleButtonElement>('geolocationToggle')!.checked);
  });

  test('GeolocationToggleChange', () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, false);

    const geolocationToggle =
        $<SettingsToggleButtonElement>('geolocationToggle')!;
    assertTrue(!!geolocationToggle);

    geolocationToggle.click();
    assertTrue(page.getPref(PrefName.GEOLOCATION_ENABLED).value);
    assertTrue(geolocationToggle.checked);

    geolocationToggle.click();
    assertFalse(page.getPref(PrefName.GEOLOCATION_ENABLED).value);
    assertFalse(geolocationToggle.checked);
  });

  test('MicrophoneToggleEnabled', () => {
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, true);

    assertTrue($<SettingsToggleButtonElement>('microphoneToggle')!.checked);
  });

  test('MicrophoneToggleDisabled', () => {
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

    assertFalse($<SettingsToggleButtonElement>('microphoneToggle')!.checked);
  });

  test('MicrophoneToggleChange', () => {
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, false);

    const microphoneToggle =
        $<SettingsToggleButtonElement>('microphoneToggle')!;
    assertTrue(!!microphoneToggle);

    microphoneToggle.click();
    assertTrue(page.getPref(PrefName.MICROPHONE_ENABLED).value);
    assertTrue(microphoneToggle.checked);

    microphoneToggle.click();
    assertFalse(page.getPref(PrefName.MICROPHONE_ENABLED).value);
    assertFalse(microphoneToggle.checked);
  });

  test('TabContextToggleEnabled', () => {
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, true);

    assertTrue($<SettingsToggleButtonElement>('tabAccessToggle')!.checked);
  });

  test('TabContextToggleDisabled', () => {
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

    assertFalse($<SettingsToggleButtonElement>('tabAccessToggle')!.checked);
  });

  test('TabContextToggleChange', () => {
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, false);

    const tabAccessToggle = $<SettingsToggleButtonElement>('tabAccessToggle');
    assertTrue(!!tabAccessToggle);

    tabAccessToggle.click();
    assertTrue(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertTrue(tabAccessToggle.checked);

    tabAccessToggle.click();
    assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertFalse(tabAccessToggle.checked);
  });

  test('TabContextExpand', async () => {
    const tabAccessToggle = $<SettingsToggleButtonElement>('tabAccessToggle')!;
    const expandButton =
        $<SettingsToggleButtonElement>('tabAccessExpandButton')!;
    const infoCard = $<CrCollapseElement>('tabAccessInfoCollapse')!;

    assertFalse(infoCard.opened);

    // Clicking the expand button opens the info card.
    expandButton.click();
    await flushTasks();
    assertTrue(infoCard.opened);
    assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);

    // Clicking the expand button again collapses the info card.
    expandButton.click();
    await flushTasks();
    assertFalse(infoCard.opened);
    assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);

    // Toggling the setting to on opens the info card.
    tabAccessToggle.click();
    await flushTasks();
    assertTrue(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertTrue(infoCard.opened);

    // Toggling the setting off closes the info card.
    tabAccessToggle.click();
    await flushTasks();
    assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertFalse(infoCard.opened);

    // Toggling the setting to on while the info card is open leaves it open.
    expandButton.click();
    await flushTasks();
    assertTrue(infoCard.opened);
    tabAccessToggle.click();
    await flushTasks();
    assertTrue(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertTrue(infoCard.opened);

    // Toggling the setting to off while the info card is closed leaves it
    // closed.
    expandButton.click();
    await flushTasks();
    assertFalse(infoCard.opened);
    tabAccessToggle.click();
    await flushTasks();
    assertFalse(page.getPref(PrefName.TAB_CONTEXT_ENABLED).value);
    assertFalse(infoCard.opened);
  });

  // Ensure the page reacts appropriately to the enterprise policy pref being
  // flipped off and back on.
  test('DisabledByPolicy', async () => {
    page.setPrefValue(PrefName.GEOLOCATION_ENABLED, true);
    page.setPrefValue(PrefName.MICROPHONE_ENABLED, true);
    page.setPrefValue(PrefName.TAB_CONTEXT_ENABLED, true);

    // Page starts off with policy enabled. The info card and activity button
    // are all present.
    assertTrue(!!$('activityButton'));
    assertTrue(!!$('tabAccessExpandButton'));
    assertTrue(!!$('tabAccessInfoCollapse'));

    // Toggles should all have values from the real pref and be enabled.
    let toggles = page.shadowRoot!.querySelectorAll(
        'settings-toggle-button[checked]:not([disabled])');
    assertEquals(3, toggles.length);

    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_DISABLED_VALUE);
    await flushTasks();

    // Now that the policy is disabled, the info card expand and activity button
    // should be removed. Toggles should all show "off" and be disabled.
    assertFalse(!!$('activityButton'));
    assertFalse(!!$('tabAccessExpandButton'));
    assertFalse(!!$('tabAccessInfoCollapse'));

    toggles = page.shadowRoot!.querySelectorAll(
        'settings-toggle-button:not([checked])[disabled]');
    assertEquals(3, toggles.length);

    // Re-enable the policy, the page should go back to the initial state.
    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);
    await flushTasks();

    assertTrue(!!$('activityButton'));
    assertTrue(!!$('tabAccessExpandButton'));
    assertTrue(!!$('tabAccessInfoCollapse'));

    toggles = page.shadowRoot!.querySelectorAll(
        'settings-toggle-button[checked]:not([disabled])');
    assertEquals(3, toggles.length);
  });
});
