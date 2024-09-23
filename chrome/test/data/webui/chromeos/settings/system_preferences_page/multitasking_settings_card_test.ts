// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/os_settings.js';

import {CrSettingsPrefs, MultitaskingSettingsCardElement, Router, routes, settingMojom, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('<multitasking-settings-card>', () => {
  let multitaskingSettingsCard: MultitaskingSettingsCardElement;

  function getFakePrefs() {
    return {
      ash: {
        snap_window_suggestions: {
          enabled: {
            key: 'ash.snap_window_suggestions.enabled',
            type: chrome.settingsPrivate.PrefType.BOOLEAN,
            value: true,
          },
        },
      },
    };
  }

  async function createCardElement(): Promise<void> {
    loadTimeData.overrideValues({
      shouldShowMultitasking: true,
    });
    multitaskingSettingsCard =
        document.createElement('multitasking-settings-card');
    multitaskingSettingsCard.prefs = getFakePrefs();
    CrSettingsPrefs.setInitialized();
    document.body.appendChild(multitaskingSettingsCard);
    await flushTasks();
  }

  function getSnapWindowSuggestionsToggle(): SettingsToggleButtonElement {
    const snapWindowSuggestionsToggle =
        multitaskingSettingsCard.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#snapWindowSuggestionsToggle');
    assertTrue(!!snapWindowSuggestionsToggle);
    return snapWindowSuggestionsToggle;
  }

  teardown(() => {
    multitaskingSettingsCard.remove();
  });

  test('Snap window toggle button updates pref value', async () => {
    await createCardElement();
    assertTrue(isVisible(multitaskingSettingsCard));

    const snapWindowSuggestionsToggle = getSnapWindowSuggestionsToggle();
    assertTrue(isVisible(snapWindowSuggestionsToggle));

    // Snap window toggle is enabled by default
    assertTrue(snapWindowSuggestionsToggle.checked);
    assertTrue(multitaskingSettingsCard.get(
        'prefs.ash.snap_window_suggestions.enabled.value'));

    // Clicking the toggle updates the pref
    snapWindowSuggestionsToggle.click();
    assertFalse(snapWindowSuggestionsToggle.checked);
    assertFalse(multitaskingSettingsCard.get(
        'prefs.ash.snap_window_suggestions.enabled.value'));
  });

  test('Snap window toggle button reflects pref value', async () => {
    function setPref(value: boolean): void {
      const newPrefs = getFakePrefs();
      newPrefs.ash.snap_window_suggestions.enabled.value = value;
      multitaskingSettingsCard.prefs = newPrefs;
    }

    await createCardElement();
    assertTrue(isVisible(multitaskingSettingsCard));

    const snapWindowSuggestionsToggle = getSnapWindowSuggestionsToggle();
    assertTrue(isVisible(snapWindowSuggestionsToggle));

    assertTrue(snapWindowSuggestionsToggle.checked);
    assertTrue(multitaskingSettingsCard.get(
        'prefs.ash.snap_window_suggestions.enabled.value'));

    setPref(false);
    assertFalse(snapWindowSuggestionsToggle.checked);

    setPref(true);
    assertTrue(snapWindowSuggestionsToggle.checked);
  });

  test('kSnapWindowSuggestions setting is deep-linkable', async () => {
    await createCardElement();

    const setting = settingMojom.Setting.kSnapWindowSuggestions;
    const params = new URLSearchParams();
    params.append('settingId', setting.toString());
    Router.getInstance().navigateTo(routes.SYSTEM_PREFERENCES, params);

    const deepLinkElement =
        multitaskingSettingsCard.shadowRoot!.querySelector<HTMLElement>(
            '#snapWindowSuggestionsToggle');
    assertTrue(!!deepLinkElement);

    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, multitaskingSettingsCard.shadowRoot!.activeElement,
        `Element should be focused for settingId=${setting}.'`);
  });
});
