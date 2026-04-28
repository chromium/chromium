// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import type {KeyboardShortcutPageElement} from 'chrome://settings/settings.js';
import {SearchEnginesBrowserProxyImpl, SearchEnginesInteractions} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {loadTimeData} from 'chrome://settings/settings.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestSearchEnginesBrowserProxy} from './test_search_engines_browser_proxy.js';

// clang-format on

suite('KeyboardShortcutPageTest', function() {
  let page: KeyboardShortcutPageElement;
  let browserProxy: TestSearchEnginesBrowserProxy;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    browserProxy = new TestSearchEnginesBrowserProxy();
    SearchEnginesBrowserProxyImpl.setInstance(browserProxy);

    loadTimeData.overrideValues({
      searchSettingsUpdate: true,
    });

    page = document.createElement('settings-keyboard-shortcut-page');
    page.prefs = {
      omnibox: {
        keyword_space_triggering_enabled: {
          key: 'omnibox.keyword_space_triggering_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
      },
    };
    document.body.appendChild(page);

    return flushTasks();
  });

  // Test that the keyboard shortcut dropdown menu is shown as expected.
  test('KeyboardShortcutSettingState', function() {
    assertTrue(isVisible(page.$.dropdown));
    assertTrue(page.getPref('omnibox.keyword_space_triggering_enabled').value);
  });

  // Test that changing the selection updates the pref and records a metric.
  test('KeyboardShortcutSettingToggle', async function() {
    const selectElement = page.$.dropdown.$.dropdownMenu;
    assertTrue(!!selectElement);

    assertTrue(page.getPref('omnibox.keyword_space_triggering_enabled').value);
    assertEquals('true', selectElement.value);

    // Switch space triggering off.
    selectElement.value = 'false';
    selectElement.dispatchEvent(new Event('change'));
    await microtasksFinished();

    assertFalse(page.getPref('omnibox.keyword_space_triggering_enabled').value);
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

    assertTrue(page.getPref('omnibox.keyword_space_triggering_enabled').value);
    assertEquals('true', selectElement.value);

    histogramResult =
        await browserProxy.whenCalled('recordSearchEnginesPageHistogram');
    assertEquals(
        SearchEnginesInteractions.KEYBOARD_SHORTCUT_SPACE_OR_TAB,
        histogramResult);
  });
});
