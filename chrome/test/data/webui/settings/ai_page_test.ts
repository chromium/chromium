// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'chrome://settings/settings.js';

import type {SettingsToggleButtonElement, SettingsAiPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {SettingsAiPageFeaturePrefName as PrefName, CrSettingsPrefs, loadTimeData, FeatureOptInState} from 'chrome://settings/settings.js';

import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {microtasksFinished, isVisible} from 'chrome://webui-test/test_util.js';

suite('ExperimentalAdvancedPage', function() {
  let page: SettingsAiPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-ai-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
  }

  // Test that interacting with the main toggle
  //  - updates the corresponding pref
  //  - updates the cr-collapse opened status
  test('MainToggle', () => {
    createPage();
    page.setPrefValue(PrefName.MAIN, FeatureOptInState.NOT_INITIALIZED);

    const mainToggle = page.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!mainToggle);
    const collapse = page.shadowRoot!.querySelector('cr-collapse');
    assertTrue(!!collapse);

    // Check NOT_INITIALIZED case.
    assertFalse(mainToggle.checked);
    assertFalse(collapse.opened);

    // Check ENABLED case.
    mainToggle.click();
    assertEquals(FeatureOptInState.ENABLED, page.getPref(PrefName.MAIN).value);
    assertTrue(mainToggle.checked);
    assertTrue(collapse.opened);

    // Check DISABLED case.
    mainToggle.click();
    assertEquals(FeatureOptInState.DISABLED, page.getPref(PrefName.MAIN).value);
    assertFalse(mainToggle.checked);
    assertFalse(collapse.opened);
  });

  test('FeaturesVisbiility', async () => {
    // Case 1, a subset of the controls should be visible.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: false,
      showHistorySearchControl: false,
    });
    createPage();

    // Turn the main pref to ENABLED so that the cr-collapse holding the
    // feature specific toggles is expanded.
    page.setPrefValue(PrefName.MAIN, FeatureOptInState.ENABLED);
    await microtasksFinished();

    let toggles =
        page.shadowRoot!.querySelectorAll('cr-collapse settings-toggle-button');
    assertEquals(3, toggles.length);
    assertTrue(isVisible(toggles[0]!));
    assertFalse(isVisible(toggles[1]!));
    assertFalse(isVisible(toggles[2]!));
    assertFalse(isVisible(page.$.historySearchRow));

    // Case 1, a different subset of the controls should be visible.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
      showHistorySearchControl: true,
    });
    createPage();

    toggles =
        page.shadowRoot!.querySelectorAll('cr-collapse settings-toggle-button');
    assertEquals(3, toggles.length);
    assertFalse(isVisible(toggles[0]!));
    assertTrue(isVisible(toggles[1]!));
    assertTrue(isVisible(toggles[2]!));
    assertTrue(isVisible(page.$.historySearchRow));
  });

  test('FeatureTogglesInteraction', () => {
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
      showHistorySearchControl: false,
    });
    createPage();
    const toggles =
        page.shadowRoot!.querySelectorAll<SettingsToggleButtonElement>(
            'cr-collapse settings-toggle-button');
    assertEquals(3, toggles.length);

    for (const toggle of toggles) {
      assertTrue(!!toggle.pref);
      page.setPrefValue(toggle.pref.key, FeatureOptInState.NOT_INITIALIZED);
      assertFalse(toggle.checked);
    }

    function assertPrefs(
        value1: FeatureOptInState, value2: FeatureOptInState,
        value3: FeatureOptInState) {
      assertEquals(value1, page.getPref(PrefName.COMPOSE).value);
      assertEquals(value2, page.getPref(PrefName.TAB_ORGANIZATION).value);
      assertEquals(value3, page.getPref(PrefName.WALLPAPER_SEARCH).value);
    }

    // Check turning on toggles one by one.
    toggles[0]!.click();
    assertPrefs(
        FeatureOptInState.ENABLED, FeatureOptInState.NOT_INITIALIZED,
        FeatureOptInState.NOT_INITIALIZED);

    toggles[1]!.click();
    assertPrefs(
        FeatureOptInState.ENABLED, FeatureOptInState.ENABLED,
        FeatureOptInState.NOT_INITIALIZED);

    toggles[2]!.click();
    assertPrefs(
        FeatureOptInState.ENABLED, FeatureOptInState.ENABLED,
        FeatureOptInState.ENABLED);

    // Check turning off toggles one by one.
    toggles[0]!.click();
    assertPrefs(
        FeatureOptInState.DISABLED, FeatureOptInState.ENABLED,
        FeatureOptInState.ENABLED);

    toggles[1]!.click();
    assertPrefs(
        FeatureOptInState.DISABLED, FeatureOptInState.DISABLED,
        FeatureOptInState.ENABLED);

    toggles[2]!.click();
    assertPrefs(
        FeatureOptInState.DISABLED, FeatureOptInState.DISABLED,
        FeatureOptInState.DISABLED);
  });

  test('FeaturesSeparators', () => {
    // Asserts whether a separator is shown for each visible row.
    function assertSeparatorsVisible(expected: boolean[]) {
      const rows = page.shadowRoot!.querySelectorAll<HTMLElement>(
          'cr-collapse settings-toggle-button:not([hidden]),' +
          'cr-link-row:not([hidden])');

      assertEquals(expected.length, rows.length);
      expected.forEach((visible, i) => {
        assertEquals(visible, rows[i]!.classList.contains('hr'));
      });
    }

    // Case1: All rows visible.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true, true, true]);

    // Case2: Row 0 hidden.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true, true]);

    // Case3: Row 1 hidden.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: true,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true, true]);

    // Case4: Row 2 hidden.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: false,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true, true]);

    // Case5: Rows 0,1 hidden.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: true,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true]);

    // Case6: Rows 0,2 hidden.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: false,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true]);

    // Case7: Rows 0-3 hidden.
    // History search always shows separator.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: false,
      showHistorySearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([true]);
  });
});
