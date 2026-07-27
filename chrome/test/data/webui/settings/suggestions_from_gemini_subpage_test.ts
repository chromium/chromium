// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {CrShortcutInputElement, SettingsSuggestionsFromGeminiSubpageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiAction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('SuggestionsFromGeminiSubpage', function() {
  let openWindowProxy: TestOpenWindowProxy;
  let settingsPrefs: SettingsPrefsElement;
  let metricsBrowserProxy: TestMetricsBrowserProxy;

  setup(async function() {
    metricsBrowserProxy = new TestMetricsBrowserProxy();
    MetricsBrowserProxyImpl.setInstance(metricsBrowserProxy);

    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    await CrSettingsPrefs.initialized;

    loadTimeData.overrideValues({
      personalContextConnectedAppsUrl: 'https://gemini.google.com/apps',
      isAtMemoryEnabled: true,
      isAtMemoryTriggerCustomizationAllowed: true,
    });

    openWindowProxy = new TestOpenWindowProxy();
    OpenWindowProxyImpl.setInstance(openWindowProxy);
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
  });

  async function setupPage():
      Promise<SettingsSuggestionsFromGeminiSubpageElement> {
    const page: SettingsSuggestionsFromGeminiSubpageElement =
        document.createElement('settings-suggestions-from-gemini-subpage');

    page.prefs = settingsPrefs.prefs!;
    page.setPrefValue(
        'autofill.at_memory.trigger_info', {is_shortcut: false, trigger: '@@'});
    page.setPrefValue('generated.find_and_fill_with_gemini', true);
    page.setPrefValue(
        'autofill.personal_context.find_and_fill_with_gemini_settings',
        ModelExecutionEnterprisePolicyValue.ALLOW);

    document.body.appendChild(page);
    await flushTasks();
    return page;
  }

  test('ManageConnectedAppsClick', async function() {
    const subpage = await setupPage();
    const row = subpage.shadowRoot!.querySelector<HTMLElement>(
        '#manageConnectedAppsLinkRow');
    assertTrue(!!row);
    assertTrue(isVisible(row));

    row.click();
    const url = await openWindowProxy.whenCalled('openUrl');
    assertEquals(
        loadTimeData.getString('personalContextConnectedAppsUrl'), url);

    const userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'PersonalContext.Settings.ManageConnectedAppsClick',
        userAction);
  });

  test('QualityLoggingRendersExpectedColumnsAndBullets', async function() {
    const subpage = await setupPage();
    const columns = subpage.shadowRoot!.querySelectorAll('.column');
    assertEquals(2, columns.length);

    const firstColumn = columns[0]!;
    const firstColumnBullets = firstColumn.querySelectorAll('li');
    assertEquals(2, firstColumnBullets.length);
    assertEquals(
        'settings20:finance',
        firstColumnBullets[0]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiWhenUsed1'),
        firstColumnBullets[0]!.querySelector(
                                  '.cr-secondary-text')!.textContent.trim());
    assertEquals(
        'settings20:personal-recommendations',
        firstColumnBullets[1]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiWhenUsed2'),
        firstColumnBullets[1]!.querySelector(
                                  '.cr-secondary-text')!.textContent.trim());

    const secondColumn = columns[1]!;
    const secondColumnBullets = secondColumn.querySelectorAll('li');
    assertEquals(3, secondColumnBullets.length);
    assertEquals(
        'settings20:insight-spark',
        secondColumnBullets[0]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider1'),
        secondColumnBullets[0]!.querySelector(
                                   '.cr-secondary-text')!.textContent.trim());
    assertEquals(
        'settings20:account-box',
        secondColumnBullets[1]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider2'),
        secondColumnBullets[1]!.querySelector(
                                   '.cr-secondary-text')!.textContent.trim());

    const considerNoLoggingEnterprise =
        subpage.shadowRoot!.querySelector('#considerNoLoggingEnterprise');
    assertTrue(!!considerNoLoggingEnterprise);
    assertFalse(isVisible(considerNoLoggingEnterprise));
  });

  test('ConsiderNoLoggingEnterpriseVisibility', async function() {
    const subpage = await setupPage();
    const considerNoLoggingEnterprise =
        subpage.shadowRoot!.querySelector('#considerNoLoggingEnterprise');
    assertTrue(!!considerNoLoggingEnterprise);

    // By default (ALLOW = 0), the bullet should be hidden.
    assertFalse(isVisible(considerNoLoggingEnterprise));

    // When policy is set to ALLOW_WITHOUT_LOGGING = 1, it should become
    // visible.
    subpage.setPrefValue(
        'autofill.personal_context.find_and_fill_with_gemini_settings',
        ModelExecutionEnterprisePolicyValue.ALLOW_WITHOUT_LOGGING);
    await flushTasks();

    assertTrue(isVisible(considerNoLoggingEnterprise));
    assertEquals(
        'cr20:domain',
        considerNoLoggingEnterprise.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider3'),
        considerNoLoggingEnterprise.querySelector('.cr-secondary-text')!
            .textContent.trim());
  });

  test('QualityLoggingIsHiddenWhenToggleIsOff', async function() {
    const subpage = await setupPage();
    assertTrue(
        isVisible(subpage.shadowRoot!.querySelector('#qualityLoggingCard')));

    subpage.set('prefs.generated.find_and_fill_with_gemini.value', false);
    await flushTasks();

    assertFalse(
        isVisible(subpage.shadowRoot!.querySelector('#qualityLoggingCard')));
  });

  test('QualityLoggingIsHiddenWhenAtMemoryDisabled', async function() {
    const subpage = await setupPage();
    assertTrue(
        isVisible(subpage.shadowRoot!.querySelector('#qualityLoggingCard')));

    subpage.set('isAtMemoryEnabled_', false);
    await flushTasks();

    assertFalse(
        isVisible(subpage.shadowRoot!.querySelector('#qualityLoggingCard')));
  });

  test('ToggleChangeRecordsMetrics', async function() {
    const subpage = await setupPage();
    const toggle = subpage.shadowRoot!.querySelector<HTMLElement>(
        '#suggestionsFromGeminiToggle');
    assertTrue(!!toggle);

    // Click toggle to turn OFF
    toggle.click();
    let action = await metricsBrowserProxy.whenCalled(
        'recordSuggestionsFromGeminiAction');
    assertEquals(SuggestionsFromGeminiAction.TOGGLE_OFF, action);
    let userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'PersonalContext.Settings.ToggledOff',
        userAction);

    metricsBrowserProxy.reset();

    // Click toggle to turn ON
    toggle.click();
    action = await metricsBrowserProxy.whenCalled(
        'recordSuggestionsFromGeminiAction');
    assertEquals(SuggestionsFromGeminiAction.TOGGLE_ON, action);
    userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals(
        'PersonalContext.Settings.ToggledOn',
        userAction);
  });

  test('AtMemoryTriggerSettingHidden', async function() {
    loadTimeData.overrideValues({
      isAtMemoryTriggerCustomizationAllowed: false,
    });
    const subpage = await setupPage();
    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryTriggerSetting cr-shortcut-input');
    assertTrue(!!inputElement);
    assertFalse(isVisible(inputElement));
  });

  test('AtMemoryTriggerSettingIsHiddenWhenToggleIsOff', async function() {
    const subpage = await setupPage();
    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryTriggerSetting cr-shortcut-input');
    assertTrue(!!inputElement);
    assertTrue(isVisible(inputElement));

    subpage.set('prefs.generated.find_and_fill_with_gemini.value', false);
    await flushTasks();

    assertFalse(isVisible(inputElement));
  });

  test('AtMemoryTriggerSettingShowsCurrentShortcut', async function() {
    const subpage = await setupPage();
    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryTriggerSetting cr-shortcut-input');
    assertTrue(!!inputElement);
    assertTrue(isVisible(inputElement));

    assertEquals(inputElement.shortcut, '');

    const shortcutString = 'Ctrl+A';
    subpage.setPrefValue(
        'autofill.at_memory.trigger_info',
        {is_shortcut: true, trigger: shortcutString});
    await flushTasks();

    assertEquals(inputElement.shortcut, shortcutString);
  });

  test('AtMemoryTriggerSettingSetsShortcut', async function() {
    const subpage = await setupPage();
    subpage.setPrefValue(
        'autofill.at_memory.trigger_info', {is_shortcut: false, trigger: '@@'});
    await flushTasks();

    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryTriggerSetting cr-shortcut-input');
    assertTrue(!!inputElement);

    inputElement.$.edit.click();
    keyDownOn(inputElement.$.input, 65, ['ctrl']);
    await flushTasks();

    const newPrefValue =
        subpage.get('prefs.autofill.at_memory.trigger_info.value');
    assertEquals(newPrefValue.trigger, 'Ctrl+A');
    assertTrue(newPrefValue.is_shortcut);
  });

  test('AtMemoryTriggerSettingClearesShortcut', async function() {
    const subpage = await setupPage();
    subpage.setPrefValue(
        'autofill.at_memory.trigger_info',
        {is_shortcut: true, trigger: 'Ctrl+A'});
    await flushTasks();

    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryTriggerSetting cr-shortcut-input');
    assertTrue(!!inputElement);

    inputElement.$.clear.click();
    await flushTasks();

    const newPrefValue =
        subpage.get('prefs.autofill.at_memory.trigger_info.value');
    assertEquals(newPrefValue.trigger, '@@');
    assertFalse(newPrefValue.is_shortcut);
  });

  test('FocusBackButton', async function() {
    const subpage = await setupPage();
    let focusCalled = false;
    const settingsSubpage =
        subpage.shadowRoot!.querySelector('settings-subpage');
    assertTrue(!!settingsSubpage);
    settingsSubpage.focusBackButton = () => {
      focusCalled = true;
      return Promise.resolve();
    };
    subpage.focusBackButton();
    assertTrue(focusCalled);
  });
});
