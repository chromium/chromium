// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import type {SettingsMouseKeysSubpageElement} from 'chrome://os-settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender, waitBeforeNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {clearBody} from '../utils.js';

suite('<settings-mouse-keys-subpage>', () => {
  let page: SettingsMouseKeysSubpageElement;
  let prefElement: SettingsPrefsElement;

  async function initPage() {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-mouse-keys-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  }

  setup(() => {
    clearBody();
    Router.getInstance().navigateTo(routes.MANAGE_MOUSE_KEYS_SETTINGS);
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Mouse keys feature disabled.', async () => {
    await initPage();

    loadTimeData.overrideValues({
      isAccessibilityMouseKeysEnabled: false,
    });

    // Toggle shouldn't be available if flag is disabled.
    const enableMouseKeysToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#enableMouseKeys');
    assertNull(enableMouseKeysToggle);
  });

  test('Mouse keys: Dominant Hand', async () => {
    await initPage();

    loadTimeData.overrideValues({
      isAccessibilityMouseKeysEnabled: true,
    });

    // If the flag is enabled, check that the UI works.
    assertFalse(page.prefs.settings.a11y.mouse_keys.enabled.value);

    // We should use primary keys by default.
    assertTrue(page.prefs.settings.a11y.mouse_keys.use_primary_keys.value);

    const enableMouseKeysToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseKeysToggle');
    assert(enableMouseKeysToggle);
    assertTrue(isVisible(enableMouseKeysToggle));

    enableMouseKeysToggle.click();
    await waitBeforeNextRender(page);
    flush();

    assertTrue(page.prefs.settings.a11y.mouse_keys.enabled.value);

    // kAccessibilityMouseKeysDominantHand
    // Ensure dominantHandControl exists.
    const dominantHandControl =
        page.shadowRoot!.querySelector<HTMLElement>(`#mouseKeysDominantHand`);
    assert(dominantHandControl);
    assertTrue(isVisible(dominantHandControl));

    // Ensure pref is set to the default value.
    let pref = page.getPref('settings.a11y.mouse_keys.dominant_hand');
    assertEquals(pref.value, 0);

    // Update dominantHandControl to alternate value.
    await waitAfterNextRender(dominantHandControl);
    const dominantHandControlElement =
        dominantHandControl.shadowRoot!.querySelector('select');
    assert(dominantHandControlElement);
    dominantHandControlElement.value = String(1);
    dominantHandControlElement.dispatchEvent(new CustomEvent('change'));

    // Ensure pref is set to the alternate value.
    pref = page.getPref('settings.a11y.mouse_keys.dominant_hand');
    assertEquals(pref.value, 1);

    // Switch to num pad.
    const usePrimaryKeysToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseKeysUsePrimaryKeys');
    assert(usePrimaryKeysToggle);
    assertTrue(isVisible(usePrimaryKeysToggle));

    usePrimaryKeysToggle.click();
    await waitBeforeNextRender(page);
    flush();

    // kAccessibilityMouseKeysUsePrimaryKeys
    assertFalse(page.prefs.settings.a11y.mouse_keys.use_primary_keys.value);

    assertFalse(isVisible(dominantHandControl));
  });

  test('Primary key toggle hides/shows primary keyboard preview', async () => {
    await initPage();

    loadTimeData.overrideValues({
      isAccessibilityMouseKeysEnabled: true,
    });

    const primaryKeysKeyboardPreview =
        page.shadowRoot!.querySelector<HTMLElement>(`#primaryKeysPreview`);

    assert(primaryKeysKeyboardPreview);
    assertTrue(isVisible(primaryKeysKeyboardPreview));

    const usePrimaryKeysToggle =
        page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
            '#mouseKeysUsePrimaryKeys');

    assert(usePrimaryKeysToggle);
    assertTrue(isVisible(usePrimaryKeysToggle));
    // Primary keys should be default enabled.
    assertTrue(page.prefs.settings.a11y.mouse_keys.use_primary_keys.value);

    // Turn primary key toggle off.
    usePrimaryKeysToggle.click();
    await waitBeforeNextRender(page);
    flush();

    assertFalse(page.prefs.settings.a11y.mouse_keys.use_primary_keys.value);
    assertFalse(isVisible(primaryKeysKeyboardPreview));

    // Turn primary key toggle on.
    usePrimaryKeysToggle.click();
    await waitBeforeNextRender(page);
    flush();

    assertTrue(page.prefs.settings.a11y.mouse_keys.use_primary_keys.value);
    assertTrue(isVisible(primaryKeysKeyboardPreview));
  });
});
