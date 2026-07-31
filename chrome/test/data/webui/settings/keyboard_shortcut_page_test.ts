// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {KeyboardShortcutPageElement} from 'chrome://settings/settings.js';
import {loadTimeData, PrefsBrowserProxy, PrefService, SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';
import {TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

suite('KeyboardShortcutPageTest', function() {
  let page: KeyboardShortcutPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;
  let prefService: PrefService;

  setup(async function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
    });

    const fakePrefs = [
      {
        key: 'omnibox.keyword_space_triggering_enabled',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: true,
      },
    ];
    const prefsBrowserProxy = new TestPrefsBrowserProxy(fakePrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    prefService = PrefService.getInstance();
    await prefService.whenInitialized();

    page = document.createElement('settings-keyboard-shortcut-page');
    document.body.appendChild(page);

    // settings-dropdown-menu updates its DOM in updated(), so we need
    // to wait an extra cycle.
    await microtasksFinished();
  });

  // Test that the keyboard shortcut dropdown menu is shown as expected.
  test('KeyboardShortcutSettingState', function() {
    assertTrue(isVisible(page.$.dropdown));
    assertTrue(
        prefService.getPref<boolean>('omnibox.keyword_space_triggering_enabled')
            .value);
  });

  // Test that changing the selection updates the pref and records a metric.
  test('KeyboardShortcutSettingToggle', async function() {
    const selectElement = page.$.dropdown.$.dropdownMenu;
    assertTrue(!!selectElement);

    assertTrue(
        prefService.getPref<boolean>('omnibox.keyword_space_triggering_enabled')
            .value);
    assertEquals('true', selectElement.value);

    // Switch space triggering off.
    selectElement.value = 'false';
    selectElement.dispatchEvent(new Event('change'));
    await microtasksFinished();

    assertFalse(
        prefService.getPref<boolean>('omnibox.keyword_space_triggering_enabled')
            .value);
    assertEquals('false', selectElement.value);

    let histogramResult =
        await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(
        SearchEnginesInteractions.KEYBOARD_SHORTCUT_TAB, histogramResult);
    browserProxy.resetResolver('recordSearchEnginesPageHistogram');

    // Switch space triggering on.
    selectElement.value = 'true';
    selectElement.dispatchEvent(new Event('change'));
    await microtasksFinished();

    assertTrue(
        prefService.getPref<boolean>('omnibox.keyword_space_triggering_enabled')
            .value);
    assertEquals('true', selectElement.value);

    histogramResult =
        await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(
        SearchEnginesInteractions.KEYBOARD_SHORTCUT_SPACE_OR_TAB,
        histogramResult);
  });
});
