// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import type {SettingsAiTabOrganizationSubpageElement} from 'chrome://settings/lazy_load.js';
import {FeatureOptInState, SettingsAiPageFeaturePrefName as PrefName} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {assertEquals, assertTrue, assertFalse} from 'chrome://webui-test/chai_assert.js';

// clang-format on

suite('TabOrganizationSubpage', function() {
  let subpage: SettingsAiTabOrganizationSubpageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  async function createPage() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    subpage = document.createElement('settings-ai-tab-organization-subpage');
    subpage.prefs = settingsPrefs.prefs;
    document.body.appendChild(subpage);
    return flushTasks();
  }

  test('tabOrganizationToggle', async () => {
    await createPage();

    const toggle = subpage.shadowRoot!.querySelector('settings-toggle-button');
    assertTrue(!!toggle);

    // Check NOT_INITIALIZED case.
    assertEquals(
        FeatureOptInState.NOT_INITIALIZED,
        subpage.getPref(PrefName.TAB_ORGANIZATION).value);
    assertFalse(toggle.checked);

    // Check ENABLED case.
    toggle.click();
    assertEquals(
        FeatureOptInState.ENABLED,
        subpage.getPref(PrefName.TAB_ORGANIZATION).value);
    assertTrue(toggle.checked);

    // Check DISABLED case.
    toggle.click();
    assertEquals(
        FeatureOptInState.DISABLED,
        subpage.getPref(PrefName.TAB_ORGANIZATION).value);
    assertFalse(toggle.checked);
  });
});
