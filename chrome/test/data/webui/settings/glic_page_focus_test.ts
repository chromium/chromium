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

import {Shortcut, TestGlicBrowserProxy} from './test_glic_browser_proxy.js';
import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

const POLICY_ENABLED_VALUE = 0;

interface Shortcuts {
  main?: string;
  focusToggle?: string;
}

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
      showAiPage: true,
      showGlicSettings: true,
    });
    resetRouterForTesting();
    return CrSettingsPrefs.initialized;
  });

  async function createGlicPage(initialShortcuts: Shortcuts) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    glicBrowserProxy = new TestGlicBrowserProxy();
    glicBrowserProxy.setGlicShortcutResponse(initialShortcuts.main || '');
    glicBrowserProxy.setGlicFocusToggleShortcutResponse(
        initialShortcuts.focusToggle || '');
    GlicBrowserProxyImpl.setInstance(glicBrowserProxy);

    page = document.createElement('settings-glic-page');
    page.prefs = settingsPrefs.prefs;
    Router.getInstance().navigateTo(routes.GEMINI);
    document.body.appendChild(page);
    await flushTasks();

    // Ensure the launcher toggle is enabled so the shortcut edit is shown.
    page.setPrefValue(PrefName.LAUNCHER_ENABLED, true);
    page.setPrefValue(PrefName.SETTINGS_POLICY, POLICY_ENABLED_VALUE);
    await microtasksFinished();
    await flushTasks();
  }

  test('ShortcutInputSuspends', async () => {
    await createGlicPage(/*initialShortcuts=*/ {
      main: '⌃A',
    });
    const shortcutInput =
        $<CrShortcutInputElement>('mainShortcutSetting .shortcut-input');
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
    await createGlicPage(/*initialShortcuts=*/ {
      main: '⌃A',
    });
    const shortcutInput =
        $<CrShortcutInputElement>('mainShortcutSetting .shortcut-input');
    assertTrue(!!shortcutInput);

    const field = shortcutInput.$.input;
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

  test('UpdateFocusToggleShortcut', async () => {
    await createGlicPage(/*initialShortcuts=*/ {
      focusToggle: 'Alt+Shift+G',
    });
    const shortcutInput =
        $<CrShortcutInputElement>('focusToggleShortcutSetting .shortcut-input');
    assertTrue(!!shortcutInput);

    const field = shortcutInput.$.input;
    assertEquals(
        1, glicBrowserProxy.getCallCount('getGlicFocusToggleShortcut'));
    assertEquals('Alt+Shift+G', shortcutInput.shortcut);

    // Clicking on the edit button should clear out the shortcut.
    glicBrowserProxy.setGlicFocusToggleShortcutResponse('');
    shortcutInput.$.edit.click();
    let arg = await glicBrowserProxy.whenCalled('setGlicFocusToggleShortcut');
    await microtasksFinished();
    assertEquals('', arg);
    assertEquals('', shortcutInput.shortcut);
    glicBrowserProxy.reset();

    // Verify that inputting an invalid shortcut doesn't update the shortcut.
    keyDownOn(field, 65);
    await microtasksFinished();
    assertEquals(
        0, glicBrowserProxy.getCallCount('setGlicFocusToggleShortcut'));
    glicBrowserProxy.reset();

    // Inputting a valid shortcut should update the shortcut.
    glicBrowserProxy.setGlicFocusToggleShortcutResponse('⌃A');
    keyDownOn(field, 65, ['ctrl']);
    arg = await glicBrowserProxy.whenCalled('setGlicFocusToggleShortcut');
    await microtasksFinished();
    assertEquals('Ctrl+A', arg);
    assertEquals('⌃A', shortcutInput.shortcut);
  });

  suite('Metrics', () => {
    let booleanHistograms: Array<[string, boolean]> = [];
    let userActions: string[] = [];

    async function assertNoBooleanHistogramsRecorded() {
      booleanHistograms =
          await metricsBrowserProxy.getArgs('recordBooleanHistogram');
      assertEquals(0, booleanHistograms.length);
    }

    async function assertNoUserActionsRecorded() {
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(0, userActions.length);
    }

    function verifyBooleanMetric(histogramName: string, visible: boolean) {
      assertTrue(booleanHistograms.some(
          histogram =>
              histogramName === histogram[0] && visible === histogram[1]));
    }

    function verifyUserAction(userAction: string) {
      assertTrue(userActions.includes(userAction));
    }

    for (const params
             of [{
               shortcutSettingId: 'mainShortcutSetting',
               initialShortcuts: {main: '⌃A'},
               customizationMetric: 'Glic.OsEntrypoint.Settings.Shortcut',
               shortcut: Shortcut.MAIN,
               enablementPrefix: 'GlicOsEntrypoint.Settings.Shortcut',
             },
                 {
                   shortcutSettingId: 'focusToggleShortcutSetting',
                   initialShortcuts: {focusToggle: '⌃A'},
                   customizationMetric:
                       'Glic.Focus.Settings.Shortcut.Customized',
                   shortcut: Shortcut.FOCUS_TOGGLE,
                 }]) {
      test(`ClearShortcut_${params.shortcutSettingId}`, async () => {
        // Arrange.
        await createGlicPage(params.initialShortcuts);
        await assertNoBooleanHistogramsRecorded();
        await assertNoUserActionsRecorded();
        const shortcutInput = $<CrShortcutInputElement>(
            `${params.shortcutSettingId} .shortcut-input`);
        assertTrue(!!shortcutInput);
        const field = shortcutInput.$.input;
        assertEquals('⌃A', field.value);

        // Act.
        shortcutInput.$.edit.click();
        await metricsBrowserProxy.whenCalled('recordBooleanHistogram');
        await microtasksFinished();
        glicBrowserProxy.setShortcutResponse(params.shortcut, /*response=*/ '');
        keyDownOn(field, 27);  // Escape key.
        await microtasksFinished();
        assertEquals('', field.value);

        // Assert.
        booleanHistograms =
            await metricsBrowserProxy.getArgs('recordBooleanHistogram');
        assertEquals(1, booleanHistograms.length);
        verifyBooleanMetric(params.customizationMetric, false);
        if (params.enablementPrefix) {
          userActions = await metricsBrowserProxy.getArgs('recordAction');
          assertEquals(1, userActions.length);
          verifyUserAction(`${params.enablementPrefix}Disabled`);
        }
      });

      test(`SetShortcut_${params.shortcutSettingId}`, async () => {
        // Arrange.
        await createGlicPage(/*initialShortcuts=*/ {});
        await assertNoBooleanHistogramsRecorded();
        await assertNoUserActionsRecorded();
        const shortcutInput = $<CrShortcutInputElement>(
            `${params.shortcutSettingId} .shortcut-input`);
        assertTrue(!!shortcutInput);
        const field = shortcutInput.$.input;
        assertEquals('', field.value);

        // Act.
        shortcutInput.$.edit.click();
        await metricsBrowserProxy.whenCalled('recordBooleanHistogram');
        await microtasksFinished();
        glicBrowserProxy.setShortcutResponse(
            params.shortcut, /*response=*/ 'Ctrl + A');
        keyDownOn(field, 65, ['ctrl']);
        await metricsBrowserProxy.whenCalled('recordBooleanHistogram');
        await microtasksFinished();

        // Assert.
        booleanHistograms =
            await metricsBrowserProxy.getArgs('recordBooleanHistogram');
        assertEquals(2, booleanHistograms.length);
        verifyBooleanMetric(params.customizationMetric, false);
        verifyBooleanMetric(params.customizationMetric, true);
        if (params.enablementPrefix) {
          userActions = await metricsBrowserProxy.getArgs('recordAction');
          assertEquals(1, userActions.length);
          verifyUserAction(`${params.enablementPrefix}Enabled`);
        }
      });

      test(`EditShortcut_${params.shortcutSettingId}`, async () => {
        // Arrange.
        await createGlicPage(params.initialShortcuts);
        await assertNoBooleanHistogramsRecorded();
        await assertNoUserActionsRecorded();
        const shortcutInput = $<CrShortcutInputElement>(
            `${params.shortcutSettingId} .shortcut-input`);
        assertTrue(!!shortcutInput);
        const field = shortcutInput.$.input;
        assertEquals('⌃A', field.value);

        // Act.
        shortcutInput.$.edit.click();
        await metricsBrowserProxy.whenCalled('recordBooleanHistogram');
        await microtasksFinished();
        glicBrowserProxy.setShortcutResponse(params.shortcut, 'Ctrl + B');
        keyDownOn(field, 66, ['ctrl']);
        await microtasksFinished();

        // Assert.
        booleanHistograms =
            await metricsBrowserProxy.getArgs('recordBooleanHistogram');
        assertEquals(2, booleanHistograms.length);
        verifyBooleanMetric(params.customizationMetric, true);
        verifyBooleanMetric(params.customizationMetric, true);
        if (params.enablementPrefix) {
          userActions = await metricsBrowserProxy.getArgs('recordAction');
          assertEquals(1, userActions.length);
          verifyUserAction(`${params.enablementPrefix}Edited`);
        }
      });
    }

    test('ToggleOSEntrypoint', async () => {
      // Arrange.
      await createGlicPage(/*initialShortcuts=*/ {
        main: '^A',
      });
      await assertNoUserActionsRecorded();
      const launcherToggle = $<SettingsToggleButtonElement>('launcherToggle');
      assertTrue(!!launcherToggle);
      assertTrue(launcherToggle.checked);

      // Act.
      launcherToggle.click();
      await microtasksFinished();

      // Assert.
      assertTrue(!launcherToggle.checked);
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(1, userActions.length);
      verifyUserAction('Glic.OsEntrypoint.Settings.Toggle.Disabled');

      // Act.
      launcherToggle.click();
      await microtasksFinished();

      // Assert.
      userActions = await metricsBrowserProxy.getArgs('recordAction');
      assertEquals(2, userActions.length);
      verifyUserAction('Glic.OsEntrypoint.Settings.Toggle.Enabled');
    });
  });
});
