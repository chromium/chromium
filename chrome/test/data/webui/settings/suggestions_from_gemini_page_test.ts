// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {CrShortcutInputElement, SettingsSuggestionsFromGeminiPageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, ModelExecutionEnterprisePolicyValue, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {MetricsBrowserProxyImpl, SuggestionsFromGeminiAction} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

import {TestMetricsBrowserProxy} from './test_metrics_browser_proxy.js';

suite('SuggestionsFromGeminiPage', function() {
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
      Promise<SettingsSuggestionsFromGeminiPageElement> {
    const page: SettingsSuggestionsFromGeminiPageElement =
        document.createElement('settings-suggestions-from-gemini-page');

    page.prefs = settingsPrefs.prefs!;
    page.setPrefValue(
        'autofill.at_memory.trigger_info', {is_shortcut: false, trigger: '@@'});
    page.setPrefValue('autofill.at_memory.double_ctrl_trigger_enabled', false);
    page.setPrefValue('autofill.at_memory.shortcut', '');
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
        'PersonalContext.Settings.ManageConnectedAppsClick', userAction);
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

    subpage.setPrefValue('generated.find_and_fill_with_gemini', false);
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
    assertEquals('PersonalContext.Settings.ToggledOff', userAction);

    metricsBrowserProxy.reset();

    // Click toggle to turn ON
    toggle.click();
    action = await metricsBrowserProxy.whenCalled(
        'recordSuggestionsFromGeminiAction');
    assertEquals(SuggestionsFromGeminiAction.TOGGLE_ON, action);
    userAction = await metricsBrowserProxy.whenCalled('recordAction');
    assertEquals('PersonalContext.Settings.ToggledOn', userAction);
  });

  test('AtMemoryTriggerSettingHidden', async function() {
    loadTimeData.overrideValues({
      isAtMemoryTriggerCustomizationAllowed: false,
    });
    const subpage = await setupPage();
    const toggleElement = subpage.$.atMemoryDoubleCtrlTriggerToggle;
    assertTrue(!!toggleElement);
    assertFalse(isVisible(toggleElement));

    const inputElement = subpage.shadowRoot!.querySelector<HTMLElement>(
        '#atMemoryShortcutSetting cr-shortcut-input');
    assertTrue(!!inputElement);
    assertFalse(isVisible(inputElement));
  });

  test('AtMemoryTriggerSettingIsHiddenWhenToggleIsOff', async function() {
    const subpage = await setupPage();
    const toggleElement = subpage.$.atMemoryDoubleCtrlTriggerToggle;
    assertTrue(!!toggleElement);
    assertTrue(isVisible(toggleElement));

    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryShortcutSetting cr-shortcut-input');
    assertTrue(!!inputElement);
    assertTrue(isVisible(inputElement));

    subpage.setPrefValue('generated.find_and_fill_with_gemini', false);
    await flushTasks();

    assertFalse(isVisible(toggleElement));
    assertFalse(isVisible(inputElement));
  });

  test('AtMemoryDoubleCtrlTriggerToggleUpdatesPref', async function() {
    const subpage = await setupPage();
    const toggleElement = subpage.$.atMemoryDoubleCtrlTriggerToggle;
    assertTrue(!!toggleElement);
    assertTrue(isVisible(toggleElement));
    assertFalse(toggleElement.checked);

    toggleElement.click();
    await flushTasks();

    assertTrue(
        subpage
            .getPref<boolean>('autofill.at_memory.double_ctrl_trigger_enabled')
            .value);
    assertTrue(toggleElement.checked);

    toggleElement.click();
    await flushTasks();

    assertFalse(
        subpage
            .getPref<boolean>('autofill.at_memory.double_ctrl_trigger_enabled')
            .value);
    assertFalse(toggleElement.checked);
  });

  test('AtMemoryTriggerSettingShowsCurrentShortcut', async function() {
    const subpage = await setupPage();
    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryShortcutSetting cr-shortcut-input');
    assertTrue(!!inputElement);
    assertTrue(isVisible(inputElement));

    assertEquals('', inputElement.shortcut);

    const shortcutString = 'Ctrl+A';
    subpage.setPrefValue('autofill.at_memory.shortcut', shortcutString);
    await flushTasks();

    assertEquals(shortcutString, inputElement.shortcut);
  });

  test('AtMemoryTriggerSettingSetsShortcut', async function() {
    const subpage = await setupPage();
    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryShortcutSetting cr-shortcut-input');
    assertTrue(!!inputElement);

    inputElement.$.edit.click();
    keyDownOn(inputElement.$.input, 65, ['ctrl']);
    await flushTasks();

    assertEquals(
        'Ctrl+A', subpage.getPref<string>('autofill.at_memory.shortcut').value);
  });

  test('AtMemoryTriggerSettingClearsShortcut', async function() {
    const subpage = await setupPage();
    subpage.setPrefValue('autofill.at_memory.shortcut', 'Ctrl+A');
    await flushTasks();

    const inputElement =
        subpage.shadowRoot!.querySelector<CrShortcutInputElement>(
            '#atMemoryShortcutSetting cr-shortcut-input');
    assertTrue(!!inputElement);

    inputElement.$.clear.click();
    await flushTasks();

    assertEquals(
        '', subpage.getPref<string>('autofill.at_memory.shortcut').value);
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
