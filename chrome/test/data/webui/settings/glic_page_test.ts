// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrShortcutInputElement} from 'chrome://settings/lazy_load.js';
import type {SettingsGlicPageElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, GlicBrowserProxyImpl, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';

const POLICY_ENABLED_VALUE = 0;
const POLICY_DISABLED_VALUE = 1;

suite('GlicPage', function() {
  let page: SettingsGlicPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let glicBrowserProxy: TestGlicBrowserProxy;

  function $<T extends HTMLElement = HTMLElement>(id: string): T|null {
    return page.shadowRoot!.querySelector<T>(`#${id}`);
  }

  async function clickToggle() {
    const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle');
    assertTrue(!!launcherToggle);
    launcherToggle.$.control.click();
    await flushTasks();
  }

  function clickToggleRow() {
    const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle');
    assertTrue(!!launcherToggle);
    launcherToggle.click();
  }

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    glicBrowserProxy = new TestGlicBrowserProxy();
    glicBrowserProxy.setGlicShortcutResponse('⌃A');
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);
    page = document.createElement('settings-glic-page');
    page.prefs = settingsPrefs.prefs;
    document.body.appendChild(page);

    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);
    return flushTasks();
  });

  test('LauncherToggleEnabled', () => {
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);

    assertTrue($<SettingsToggleButtonElement>('launcherToggle')!.checked);
  });

  test('LauncherToggleDisabled', () => {
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);

    assertFalse($<SettingsToggleButtonElement>('launcherToggle')!.checked);
  });

  for (const clickType of [clickToggle, clickToggleRow]) {
    const clickTypeName = clickType.name.replace('click', '');
    test('Launcher' + clickTypeName + 'Change', async () => {
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);

      const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle')!;

      await clickType();
      assertTrue(page.getPref(PrefName.LAUNCHER_ENABLED).value);
      assertTrue(launcherToggle.checked);
      assertEquals(
          1, glicBrowserProxy.getCallCount('setGlicOsLauncherEnabled'));
      glicBrowserProxy.reset();

      await clickType();
      assertFalse(page.getPref(PrefName.LAUNCHER_ENABLED).value);
      assertFalse(launcherToggle.checked);
      assertEquals(
          1, glicBrowserProxy.getCallCount('setGlicOsLauncherEnabled'));
      glicBrowserProxy.reset();
    });

    // Test that the keyboard shortcut is collapsed/invisible when the launcher
    // is disabled and shown when the launcher is enabled.
    test('KeyboardShortcutVisibility' + clickTypeName, async () => {
      const keyboardShortcutSetting = $('keyboardShortcutSetting');

      // The pref starts off disabled, the keyboard shortcut row should be
      // hidden.
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, false);
      assertFalse(isVisible(keyboardShortcutSetting));

      // Enable using the launcher toggle, the row should show.
      await clickType();
      assertTrue(page.getPref(PrefName.LAUNCHER_ENABLED).value);
      assertTrue(isVisible(keyboardShortcutSetting));

      // Disable using the launcher toggle, the row should hide.
      await clickType();
      assertFalse(page.getPref(PrefName.LAUNCHER_ENABLED).value);
      assertFalse(isVisible(keyboardShortcutSetting));

      // Enable via pref, the row should show.
      page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
      await flushTasks();
      assertTrue(isVisible(keyboardShortcutSetting));
    });
  }

  test('ShortcutInputSuspends', async () => {
    const shortcutInput = $<CrShortcutInputElement>('shortcutInput')!;

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
    const shortcutInput = $<CrShortcutInputElement>('shortcutInput')!;
    const field = shortcutInput.$.input;
    await microtasksFinished();
    assertEquals(1, glicBrowserProxy.getCallCount('getGlicShortcut'));
    assertEquals('⌃A', shortcutInput.shortcut);

    // Clicking on the edit button should clear out the shortcut.
    glicBrowserProxy.setGlicShortcutResponse('');
    shortcutInput.$.edit.click();
    let arg = await glicBrowserProxy.whenCalled('setGlicShortcut');
    await microtasksFinished();
    assertEquals('', arg);
    assertEquals('', shortcutInput.shortcut);
    glicBrowserProxy.reset();

    // Verify that inputting an invalid shortcut doesn't update the shortcut.
    keyDownOn(field, 65);
    await microtasksFinished();
    assertEquals(0, glicBrowserProxy.getCallCount('setGlicShortcut'));
    glicBrowserProxy.reset();

    // Inputting a valid shortcut should update the shortcut.
    glicBrowserProxy.setGlicShortcutResponse('⌃A');
    keyDownOn(field, 65, ['ctrl']);
    arg = await glicBrowserProxy.whenCalled('setGlicShortcut');
    await microtasksFinished();
    assertEquals('Ctrl+A', arg);
    assertEquals('⌃A', shortcutInput.shortcut);
  });

  // Ensure the page reacts appropriately to the enterprise policy pref being
  // flipped off and back on.
  test('DisabledByPolicy', async () => {
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);

    // Page starts with the policy enabled. The controls should be connected and
    // visible.
    assertTrue(!!$('launcherToggle'));
    assertTrue(!!$('shortcutInput'));
    assertTrue(isVisible($('shortcutInput')));

    // The toggle should show the value from the real pref and be enabled.
    let toggles = page.shadowRoot!.querySelectorAll(
        'settings-toggle-button[checked]:not([disabled])');
    assertEquals(1, toggles.length);

    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_DISABLED_VALUE);
    await flushTasks();

    // The shortcut input should be removed and launcher toggle off and
    // disabled.
    assertFalse(!!$('shortcutInput'));
    toggles = page.shadowRoot!.querySelectorAll(
        'settings-toggle-button:not([checked])[disabled]');
    assertEquals(1, toggles.length);

    // Re-enable the policy, the page should go back to the initial state.
    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);
    await flushTasks();

    assertTrue(!!$('launcherToggle'));
    assertTrue(!!$('shortcutInput'));
    assertTrue(isVisible($('shortcutInput')));
    toggles = page.shadowRoot!.querySelectorAll(
        'settings-toggle-button[checked]:not([disabled])');
    assertEquals(1, toggles.length);
  });
});
