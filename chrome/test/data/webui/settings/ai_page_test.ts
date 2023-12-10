// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'chrome://settings/settings.js';

import {LifetimeBrowserProxyImpl, SettingsAiPageFeaturePrefName as PrefName, SettingsToggleButtonElement, SettingsAiPageElement, CrSettingsPrefs, SettingsPrefsElement, loadTimeData, FeatureOptInState} from 'chrome://settings/settings.js';

import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';
import {TestLifetimeBrowserProxy} from './test_lifetime_browser_proxy.js';

suite('ExperimentalAdvancedPage', function() {
  let page: SettingsAiPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let lifetimeBrowserProxy: TestLifetimeBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    lifetimeBrowserProxy = new TestLifetimeBrowserProxy();
    LifetimeBrowserProxyImpl.setInstance(lifetimeBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-ai-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
  }

  // Test that interacting with the main toggle
  //  - updates the corresponding pref
  //  - updates the iron-collapse opened status
  //  - shows the restart toast
  test('MainToggle', () => {
    createPage();
    page.setPrefValue(PrefName.MAIN, FeatureOptInState.NOT_INITIALIZED);

    const mainToggle = page.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!mainToggle);
    const collapse = page.shadowRoot!.querySelector('iron-collapse');
    assertTrue(!!collapse);

    // Check NOT_INITIALIZED case.
    assertFalse(mainToggle.checked);
    assertFalse(collapse.opened);
    assertFalse(page.$.toast.open);

    // Check ENABLED case.
    mainToggle.click();
    assertEquals(FeatureOptInState.ENABLED, page.getPref(PrefName.MAIN).value);
    assertTrue(mainToggle.checked);
    assertTrue(collapse.opened);
    assertTrue(page.$.toast.open);

    // Check DISABLED case.
    mainToggle.click();
    assertEquals(FeatureOptInState.DISABLED, page.getPref(PrefName.MAIN).value);
    assertFalse(mainToggle.checked);
    assertFalse(collapse.opened);
    assertTrue(page.$.toast.open);
  });

  test('FeatureTogglesVisibility', () => {
    // Case 1, a subset of the controls should be visible.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: false,
    });
    createPage();

    // Turn the main pref to ENABLED so that the iron-collapse holding the
    // feature specific toggles is expanded.
    page.setPrefValue(PrefName.MAIN, FeatureOptInState.ENABLED);

    let toggles = page.shadowRoot!.querySelectorAll(
        'iron-collapse settings-toggle-button');
    assertEquals(3, toggles.length);
    assertTrue(isVisible(toggles[0]!));
    assertFalse(isVisible(toggles[1]!));
    assertFalse(isVisible(toggles[2]!));

    // Case 1, a different subset of the controls should be visible.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
    });
    createPage();

    toggles = page.shadowRoot!.querySelectorAll(
        'iron-collapse settings-toggle-button');
    assertEquals(3, toggles.length);
    assertFalse(isVisible(toggles[0]!));
    assertTrue(isVisible(toggles[1]!));
    assertTrue(isVisible(toggles[2]!));
  });

  test('FeatureTogglesInteraction', () => {
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
    });
    createPage();
    const toggles =
        page.shadowRoot!.querySelectorAll<SettingsToggleButtonElement>(
            'iron-collapse settings-toggle-button');
    assertEquals(3, toggles.length);
    assertFalse(page.$.toast.open);

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
    assertTrue(page.$.toast.open);

    // Manually hide toast to test the next toggle, normally stays open.
    page.$.toast.hide();

    toggles[1]!.click();
    assertPrefs(
        FeatureOptInState.ENABLED, FeatureOptInState.ENABLED,
        FeatureOptInState.NOT_INITIALIZED);

    toggles[2]!.click();
    assertPrefs(
        FeatureOptInState.ENABLED, FeatureOptInState.ENABLED,
        FeatureOptInState.ENABLED);
    assertTrue(page.$.toast.open);
    page.$.toast.hide();

    // Check turning off toggles one by one.
    toggles[0]!.click();
    assertPrefs(
        FeatureOptInState.DISABLED, FeatureOptInState.ENABLED,
        FeatureOptInState.ENABLED);
    assertTrue(page.$.toast.open);
    page.$.toast.hide();

    toggles[1]!.click();
    assertPrefs(
        FeatureOptInState.DISABLED, FeatureOptInState.DISABLED,
        FeatureOptInState.ENABLED);
    assertTrue(page.$.toast.open);
    page.$.toast.hide();

    toggles[2]!.click();
    assertPrefs(
        FeatureOptInState.DISABLED, FeatureOptInState.DISABLED,
        FeatureOptInState.DISABLED);
    assertTrue(page.$.toast.open);
  });

  test('RelaunchToast', () => {
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
    });
    createPage();
    const toggle = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        'iron-collapse settings-toggle-button');
    assertTrue(!!toggle);
    assertFalse(toggle.checked);
    assertFalse(page.$.toast.open);

    toggle.click();
    assertTrue(page.$.toast.open);
    const restartButton = page.$.toast.querySelector('cr-button');
    assertTrue(!!restartButton);

    restartButton.click();
    return lifetimeBrowserProxy.whenCalled('restart');
  });

  test('FeatureTogglesSeparators', () => {
    // Asserts whether a separator is shown for each visible row.
    function assertSeparatorsVisible(expected: boolean[]) {
      const toggles =
          page.shadowRoot!.querySelectorAll<SettingsToggleButtonElement>(
              'iron-collapse settings-toggle-button:not([hidden])');

      assertEquals(expected.length, toggles.length);
      expected.forEach((visible, i) => {
        assertEquals(visible, toggles[i]!.classList.contains('hr'));
      });
    }

    // Case1: All rows visible.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true, true]);

    // Case2: Row 0 hidden.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true]);

    // Case3: Row 1 hidden.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false, true]);

    // Case4: Row 2 hidden.
    loadTimeData.overrideValues({
      showComposeControl: true,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: false,
    });
    createPage();
    assertSeparatorsVisible([false, true]);

    // Case5: Rows 0,1 hidden.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: true,
    });
    createPage();
    assertSeparatorsVisible([false]);

    // Case6: Rows 0,2 hidden.
    loadTimeData.overrideValues({
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: false,
    });
    createPage();
    assertSeparatorsVisible([false]);
  });
});
