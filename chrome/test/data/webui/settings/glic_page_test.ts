// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsGlicPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

suite('GlicPage', function() {
  let page: SettingsGlicPageElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    page = document.createElement('settings-glic-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);
    return flushTasks();
  });

  test('LauncherToggleEnabled', () => {
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);

    assertTrue(page.$.launcherToggle.checked);
  });

  test('LauncherToggleDisabled', () => {
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);

    assertFalse(page.$.launcherToggle.checked);
  });

  test('LauncherToggleChange', () => {
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);

    const launcherToggle = page.$.launcherToggle;
    assertTrue(!!launcherToggle);

    launcherToggle.click();
    assertEquals(true, page.getPref(PrefName.LAUNCHER_ENABLED).value);
    assertTrue(launcherToggle.checked);

    launcherToggle.click();
    assertEquals(false, page.getPref(PrefName.LAUNCHER_ENABLED).value);
    assertFalse(launcherToggle.checked);
  });
});
