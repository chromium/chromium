// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import type {CrShortcutInputElement, SettingsSuggestionsFromGeminiSubpageElement} from 'chrome://settings/lazy_load.js';
import {CrSettingsPrefs, loadTimeData, OpenWindowProxyImpl} from 'chrome://settings/settings.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestOpenWindowProxy} from 'chrome://webui-test/test_open_window_proxy.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('SuggestionsFromGeminiSubpage', function() {

  let openWindowProxy: TestOpenWindowProxy;
  let settingsPrefs: SettingsPrefsElement;

  setup(async function() {
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
    page.setPrefValue('autofill.personal_context.settings_toggle_status', true);

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
    assertEquals(
        'cr20:domain', secondColumnBullets[2]!.querySelector('cr-icon')!.icon);
    assertEquals(
        loadTimeData.getString('suggestionsFromGeminiConsider3'),
        secondColumnBullets[2]!.querySelector(
                                   '.cr-secondary-text')!.textContent.trim());
  });

  test('QualityLoggingIsHiddenWhenToggleIsOff', async function() {
    const subpage = await setupPage();
    assertTrue(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));

    subpage.set(
        'prefs.autofill.personal_context.settings_toggle_status.value', false);
    await flushTasks();

    assertFalse(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));
  });

  test('QualityLoggingIsHiddenWhenAtMemoryDisabled', async function() {
    const subpage = await setupPage();
    assertTrue(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));

    subpage.set('isAtMemoryEnabled_', false);
    await flushTasks();

    assertFalse(!!subpage.shadowRoot!.querySelector('#qualityLoggingCard'));
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
});
