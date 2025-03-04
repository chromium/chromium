// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrShortcutInputElement} from 'chrome://settings/lazy_load.js';
import type {SettingsGlicPageElement, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, GlicBrowserProxyImpl, loadTimeData, resetRouterForTesting, Router, routes, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';

const POLICY_ENABLED_VALUE = 0;

suite('GlicPageFocusTest', function() {
  let page: SettingsGlicPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let glicBrowserProxy: TestGlicBrowserProxy;

  function $<T extends HTMLElement = HTMLElement>(id: string): T|null {
    return page.shadowRoot!.querySelector<T>(`#${id}`);
  }

  suiteSetup(function() {
    settingsPrefs = document.createElement('settings-prefs');
    loadTimeData.overrideValues({
      showAdvancedFeaturesMainControl: true,
      showGlicSettings: true,
    });
    resetRouterForTesting();
    return CrSettingsPrefs.initialized;
  });

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    glicBrowserProxy = new TestGlicBrowserProxy();
    glicBrowserProxy.setGlicShortcutResponse('⌃A');
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);

    page = document.createElement('settings-glic-page');
    page.prefs = settingsPrefs.prefs;
    Router.getInstance().navigateTo(routes.GEMINI);
    document.body.appendChild(page);

    // Ensure the launcher toggle is enabled so the shortcut edit is shown.
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);

    return flushTasks();
  });

  test('ShortcutInputSuspends', async () => {
    const shortcutInput = $<CrShortcutInputElement>('shortcutInput');
    assertTrue(!!shortcutInput);

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
    const shortcutInput = $<CrShortcutInputElement>('shortcutInput');
    assertTrue(!!shortcutInput);

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
});
