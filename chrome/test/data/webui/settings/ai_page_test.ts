// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'chrome://settings/settings.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import type {SettingsToggleButtonElement, SettingsAiPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, loadTimeData, resetRouterForTesting, Router, routes, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {microtasksFinished, isChildVisible, isVisible} from 'chrome://webui-test/test_util.js';

suite('ExperimentalAdvancedPage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let page: SettingsAiPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-ai-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    return flushTasks();
  }

  test('FeaturesVisibilityWithRefreshEnabled', async () => {
    // Case 1, a subset of the controls should be visible.
    loadTimeData.overrideValues({
      showHistorySearchControl: false,
      showComposeControl: true,
      showTabOrganizationControl: false,
      showWallpaperSearchControl: false,
    });
    resetRouterForTesting();
    await createPage();

    assertFalse(isChildVisible(page, '#historySearchRowV2'));
    assertTrue(isChildVisible(page, '#composeRowV2'));
    assertFalse(isChildVisible(page, '#tabOrganizationRowV2'));
    assertFalse(isChildVisible(page, '#wallpaperSearchRowV2'));

    // The old UI should not be visible if the refresh flag is enabled.
    const toggles1 =
        page.shadowRoot!.querySelectorAll('settings-toggle-button');
    assertEquals(0, toggles1.length);
    assertFalse(isChildVisible(page, '#historySearchRow'));

    // Case 2, a different subset of the controls should be visible.
    loadTimeData.overrideValues({
      showHistorySearchControl: true,
      showComposeControl: false,
      showTabOrganizationControl: true,
      showWallpaperSearchControl: true,
    });
    resetRouterForTesting();
    await createPage();

    assertTrue(isChildVisible(page, '#historySearchRowV2'));
    assertFalse(isChildVisible(page, '#composeRowV2'));
    assertTrue(isChildVisible(page, '#tabOrganizationRowV2'));
    assertTrue(isChildVisible(page, '#wallpaperSearchRowV2'));

    // The old UI should not be visible if the refresh flag is enabled.
    const toggles2 =
        page.shadowRoot!.querySelectorAll('settings-toggle-button');
    assertEquals(0, toggles2.length);
    assertFalse(isChildVisible(page, '#historySearchRow'));
  });

  test('historySearchRow', async () => {
    loadTimeData.overrideValues({
      showAdvancedFeaturesMainControl: true,
      showHistorySearchControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const historySearchRow =
        page.shadowRoot!.querySelector<HTMLElement>('#historySearchRowV2');

    assertTrue(!!historySearchRow);
    assertTrue(isVisible(historySearchRow));
    historySearchRow.click();

    const currentRoute = Router.getInstance().getCurrentRoute();
    assertEquals(routes.HISTORY_SEARCH, currentRoute);
    assertEquals(routes.AI, currentRoute.parent);
  });

  test('composeRow', async () => {
    loadTimeData.overrideValues({
      showAdvancedFeaturesMainControl: true,
      showComposeControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const composeRow =
        page.shadowRoot!.querySelector<HTMLElement>('#composeRowV2');

    assertTrue(!!composeRow);
    assertTrue(isVisible(composeRow));
    composeRow.click();

    const currentRoute = Router.getInstance().getCurrentRoute();
    assertEquals(routes.OFFER_WRITING_HELP, currentRoute);
    assertEquals(routes.AI, currentRoute.parent);
  });

  test('tabOrganizationRow', async () => {
    loadTimeData.overrideValues({
      showAdvancedFeaturesMainControl: true,
      showTabOrganizationControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const tabOrganizationRow =
        page.shadowRoot!.querySelector<HTMLElement>('#tabOrganizationRowV2');

    assertTrue(!!tabOrganizationRow);
    assertTrue(isVisible(tabOrganizationRow));
    tabOrganizationRow.click();
    assertEquals(
        routes.AI_TAB_ORGANIZATION, Router.getInstance().getCurrentRoute());
  });

  test('WallpaperSearchRow', async () => {
    loadTimeData.overrideValues({
      showWallpaperSearchControl: true,
    });
    resetRouterForTesting();
    await createPage();

    const wallpaperSearchRow =
        page.shadowRoot!.querySelector<HTMLElement>('#wallpaperSearchRowV2');
    assertTrue(!!wallpaperSearchRow);
    assertTrue(isVisible(wallpaperSearchRow));

    wallpaperSearchRow.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(url, loadTimeData.getString('wallpaperSearchLearnMoreUrl'));
  });
});

// TODO(crbug.com/362225975): Remove after AiSettingsPageRefresh is launched.
suite('ExperimentalAdvancedPageRefreshDisabled', () => {
  let page: SettingsAiPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    loadTimeData.overrideValues({enableAiSettingsPageRefresh: false});

    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-ai-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    flush();
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

  test('FeaturesVisibility', async () => {
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
    assertFalse(isChildVisible(page, '#historySearchRow'));

    // V2 UI should be hidden if refresh flag is disabled.
    assertFalse(isChildVisible(page, '#historySearchRowV2'));
    assertFalse(isChildVisible(page, '#composeRowV2'));
    assertFalse(isChildVisible(page, '#tabOrganizationRowV2'));
    assertFalse(isChildVisible(page, '#wallpaperSearchRowV2'));

    // Case 2, a different subset of the controls should be visible.
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
    assertTrue(isChildVisible(page, '#historySearchRow'));

    // V2 UI should be hidden if refresh flag is disabled.
    assertFalse(isChildVisible(page, '#historySearchRowV2'));
    assertFalse(isChildVisible(page, '#composeRowV2'));
    assertFalse(isChildVisible(page, '#tabOrganizationRowV2'));
    assertFalse(isChildVisible(page, '#wallpaperSearchRowV2'));
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
