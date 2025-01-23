// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {SettingsGlicPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, GlicBrowserProxyImpl, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';

suite('GlicPage', function() {
  let page: SettingsGlicPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let glicBrowserProxy: TestGlicBrowserProxy;

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    glicBrowserProxy = new TestGlicBrowserProxy();
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);
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

  test('ShortcutInputSuspends', async () => {
    const shortcutInput = page.$.shortcutInput;

    // Clicking on the edit button should suspend shortcuts because the input is
    // waiting for a new shortcut to save
    shortcutInput.$.edit.click();
    let arg = await glicBrowserProxy.whenCalled('setShortcutSuspensionState');
    assertTrue(arg);
    glicBrowserProxy.reset();

    // Pressing the escape key should re-enable shortcuts since the input is no
    // longer waiting for a shortcut to save
    shortcutInput.$.edit.click();
    keyDownOn(shortcutInput.$.input, 27);  // Escape key.
    arg = await glicBrowserProxy.whenCalled('setShortcutSuspensionState');
    assertFalse(arg);
  });

  test('UpdateShortcut', async () => {
    const shortcutInput = page.$.shortcutInput;
    const field = shortcutInput.$.input;
    await microtasksFinished();
    assertEquals(1, glicBrowserProxy.getCallCount('getGlicShortcut'));
    assertEquals('Ctrl+A', shortcutInput.shortcut);

    // Clicking on the edit button should clear out the shortcut.
    shortcutInput.$.edit.click();
    let arg = await glicBrowserProxy.whenCalled('setGlicShortcut');
    assertEquals('', arg);
    glicBrowserProxy.reset();

    // Verify that inputting an invalid shortcut doesn't update the shortcut.
    keyDownOn(field, 65);
    await microtasksFinished();
    assertEquals(0, glicBrowserProxy.getCallCount('setGlicShortcut'));
    glicBrowserProxy.reset();

    // Inputting a valid shortcut should update the shortcut.
    keyDownOn(field, 65, ['ctrl']);
    arg = await glicBrowserProxy.whenCalled('setGlicShortcut');
    assertEquals('Ctrl+A', arg);
  });
});
