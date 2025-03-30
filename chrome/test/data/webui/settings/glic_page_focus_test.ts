// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/settings.js';

import type {CrShortcutInputElement} from 'chrome://settings/lazy_load.js';
import type {SettingsGlicPageElement, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs, GlicBrowserProxyImpl, loadTimeData, MetricsBrowserProxyImpl, resetRouterForTesting, Router, routes, SettingsGlicPageFeaturePrefName as PrefName} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestGlicBrowserProxy} from './test_glic_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

const POLICY_ENABLED_VALUE = 0;

suite('GlicPageFocusTest', function() {
  let page: SettingsGlicPageElement;
  let settingsPrefs: SettingsPrefsElement;
  let glicBrowserProxy: TestGlicBrowserProxy;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

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

  function createGlicPage(initialShortcut: string) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    glicBrowserProxy = new TestGlicBrowserProxy();
    glicBrowserProxy.setGlicShortcutResponse(initialShortcut);
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);

    page = document.createElement('settings-glic-page');
    page.prefs = settingsPrefs.prefs;
    Router.getInstance().navigateTo(routes.GEMINI);
    document.body.appendChild(page);

    // Ensure the launcher toggle is enabled so the shortcut edit is shown.
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);

    return flushTasks();
  }

  setup(function() {
    return createGlicPage(/*initialShortcut=*/ '⌃A');
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

  suite('Metrics', () => {
    let booleanHistograms: Array<[string, boolean]> = [];
    let userActions: string[] = [];

    function verifyBooleanMetric(histogramName: string, visible: boolean) {
      assertTrue(booleanHistograms.some(
          histogram =>
              histogramName === histogram[0] && visible === histogram[1]));
    }

    function verifyUserAction(userAction: string) {
      assertTrue(userActions.includes(userAction));
    }

    test('clear shortcut', async () => {
      // Arrange.
      const shortcutInput = $<CrShortcutInputElement>('shortcutInput');
      assertTrue(!!shortcutInput);
      const field = shortcutInput.$.input;
      assertEquals('⌃A', field.value);
      // Clear any toggle-related metrics.
      metricsBrowserProxy.reset();

      // Act.
      glicBrowserProxy.setGlicShortcutResponse('');
      shortcutInput.$.edit.click();
      await flushTasks();
      keyDownOn(field, 27);  // Escape key.
      await flushTasks();
      assertEquals('', field.value);

      // Assert.
      booleanHistograms =
          await metricsBrowserProxy.getArgs('recordBooleanHistogram');
      assertEquals(1, booleanHistograms.length);
      const hasValue = 'Glic.OsEntrypoint.Settings.Shortcut';
      verifyBooleanMetric(hasValue, false);
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(1, userActions.length);
      verifyUserAction('GlicOsEntrypoint.Settings.ShortcutDisabled');
    });

    test('set shortcut', async () => {
      // Arrange.
      await createGlicPage(/*initialShortcut=*/ '');

      // Flush again since creating the page queues a task to focus the back
      // button. If we proceed before that runs it'll steal focus which
      // interferes with the test.
      await flushTasks();

      const shortcutInput = $<CrShortcutInputElement>('shortcutInput');
      assertTrue(!!shortcutInput);
      const field = shortcutInput.$.input;
      assertEquals('', field.value);
      // Clear any toggle-related metrics.
      metricsBrowserProxy.reset();

      // Act.
      glicBrowserProxy.setGlicShortcutResponse('⌃A');
      shortcutInput.$.edit.click();
      await flushTasks();
      keyDownOn(field, 65, ['ctrl']);
      await flushTasks();

      // Assert.
      booleanHistograms =
          await metricsBrowserProxy.getArgs('recordBooleanHistogram');
      assertEquals(2, booleanHistograms.length);
      const hasValue = 'Glic.OsEntrypoint.Settings.Shortcut';
      verifyBooleanMetric(hasValue, false);
      verifyBooleanMetric(hasValue, true);
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(1, userActions.length);
      verifyUserAction('GlicOsEntrypoint.Settings.ShortcutEnabled');
    });

    test('edit shortcut', async () => {
      // Flush task for focusing on the back button.
      await flushTasks();

      // Assert.
      booleanHistograms =
          await metricsBrowserProxy.getArgs('recordBooleanHistogram');
      assertEquals(0, booleanHistograms.length);

      // Arrange.
      const shortcutInput = $<CrShortcutInputElement>('shortcutInput');
      assertTrue(!!shortcutInput);
      const field = shortcutInput.$.input;
      assertEquals('⌃A', field.value);

      // Act.
      shortcutInput.$.edit.click();
      await metricsBrowserProxy.whenCalled('recordBooleanHistogram');
      await flushTasks();
      glicBrowserProxy.setGlicShortcutResponse('Ctrl + B');
      keyDownOn(field, 66, ['ctrl']);
      await flushTasks();

      // Assert.
      booleanHistograms =
          await metricsBrowserProxy.getArgs('recordBooleanHistogram');
      assertEquals(2, booleanHistograms.length);
      const hasValue = 'Glic.OsEntrypoint.Settings.Shortcut';
      verifyBooleanMetric(hasValue, true);
      verifyBooleanMetric(hasValue, true);
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(1, userActions.length);
      verifyUserAction('GlicOsEntrypoint.Settings.ShortcutEdited');
    });

    test('toggle OS entrypoint', async () => {
      // Assert no actions are logged upon load.
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(0, userActions.length);

      // Arrange.
      const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle');
      assertTrue(!!launcherToggle);
      assertTrue(launcherToggle.checked);

      // Act.
      launcherToggle.click();
      await flushTasks();

      // Assert.
      assertTrue(!launcherToggle.checked);
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(1, userActions.length);
      verifyUserAction('Glic.OsEntrypoint.Settings.Toggle.Disabled');

      // Act.
      launcherToggle.click();
      await flushTasks();

      // Assert.
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(2, userActions.length);
      verifyUserAction('Glic.OsEntrypoint.Settings.Toggle.Enabled');
    });
  });
});
