// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsGlicPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

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

    launcherToggle.click();
    assertTrue(page.getPref(PrefName.LAUNCHER_ENABLED).value);
    assertTrue(launcherToggle.checked);

    launcherToggle.click();
    assertFalse(page.getPref(PrefName.LAUNCHER_ENABLED).value);
    assertFalse(launcherToggle.checked);
  });

  // Test that the keyboard shortcut is collapsed/invisible when the launcher
  // is disabled and shown when the launcher is enabled.
  test('KeyboardShortcutVisibility', async () => {
    const launcherToggle = page.$.launcherToggle;
    const keyboardShortcutSetting = page.$.keyboardShortcutSetting;

    // The pref starts off disabled, the keyboard shortcut row should be hidden.
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);
    assertFalse(isVisible(keyboardShortcutSetting));

    // Enable using the launcher toggle, the row should show.
    launcherToggle.click();
    await flushTasks();
    assertTrue(page.getPref(PrefName.LAUNCHER_ENABLED).value);
    assertTrue(isVisible(keyboardShortcutSetting));

    // Disable using the launcher toggle, the row should hide.
    launcherToggle.click();
    await flushTasks();
    assertFalse(page.getPref(PrefName.LAUNCHER_ENABLED).value);
    assertFalse(isVisible(keyboardShortcutSetting));

    // Enable via pref, the row should show.
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
    await flushTasks();
    assertTrue(isVisible(keyboardShortcutSetting));
  });
});
