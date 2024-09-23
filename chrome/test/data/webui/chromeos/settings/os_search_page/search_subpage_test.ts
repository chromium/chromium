// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsSearchSubpageElement} from 'chrome://os-settings/lazy_load.js';
import {CrSettingsPrefs, Router, routes, SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

suite('<settings-search-subpage>', () => {
  let page: SettingsSearchSubpageElement;
  let prefElement: SettingsPrefsElement;

  suiteSetup(() => {
    loadTimeData.overrideValues({
      isQuickAnswersSupported: true,
      quickAnswersSubToggleEnabled: true,
    });
  });

  setup(async () => {
    prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    await CrSettingsPrefs.initialized;
    page = document.createElement('settings-search-subpage');
    page.prefs = prefElement.prefs;
    document.body.appendChild(page);
    flush();
  });

  teardown(() => {
    page.remove();
    prefElement.remove();
    CrSettingsPrefs.resetForTesting();
  });

  test('definitionToggleVisibility', () => {
    let button =
        page.shadowRoot!.querySelector('#quick-answers-definition-enable');
    assertEquals(null, button);

    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    button = page.shadowRoot!.querySelector('#quick-answers-definition-enable');
    assert(button);
  });

  test('translationToggleVisibility', () => {
    let button =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable');
    assertEquals(null, button);

    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    button =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable');
    assert(button);
  });

  test('unitConversionToggleVisibility', () => {
    let button =
        page.shadowRoot!.querySelector('#quick-answers-unit-conversion-enable');
    assertEquals(null, button);

    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    button =
        page.shadowRoot!.querySelector('#quick-answers-unit-conversion-enable');
    assert(button);
  });

  test('toggleQuickAnswers', () => {
    flush();
    const button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#quick-answers-enable');
    assert(button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    let definitionButton =
        page.shadowRoot!.querySelector('#quick-answers-definition-enable');
    let translationButton =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable');
    let unitConversionButton =
        page.shadowRoot!.querySelector('#quick-answers-unit-conversion-enable');
    assertEquals(null, definitionButton);
    assertEquals(null, translationButton);
    assertEquals(null, unitConversionButton);

    // Tap the enable toggle button and ensure the state becomes enabled.
    button.click();
    flush();
    assertTrue(button.checked);

    definitionButton =
        page.shadowRoot!.querySelector('#quick-answers-definition-enable');
    translationButton =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable');
    unitConversionButton =
        page.shadowRoot!.querySelector('#quick-answers-unit-conversion-enable');
    assert(definitionButton);
    assert(translationButton);
    assert(unitConversionButton);
  });

  test('toggleQuickAnswersDefinition', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#quick-answers-definition-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.definition.enabled', false);
    flush();

    button = page.shadowRoot!.querySelector('#quick-answers-definition-enable');
    assert(button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(page.getPref('settings.quick_answers.definition.enabled').value);
  });

  test('toggleQuickAnswersTranslation', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#quick-answers-translation-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.translation.enabled', false);
    flush();

    button =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable');
    assert(button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.quick_answers.translation.enabled').value);
  });

  test('clickLanguageSettingsLink', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#quick-answers-translation-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.translation.enabled', false);
    flush();

    button =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable');
    assert(button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    const languageSettingsLink =
        button.shadowRoot!.querySelector(
                              '#sub-label-text-with-link')!.querySelector('a');
    assert(languageSettingsLink);

    languageSettingsLink.click();
    flush();
    assertFalse(button.checked);
    assertFalse(
        page.getPref('settings.quick_answers.translation.enabled').value);

    assertEquals(
        routes.OS_LANGUAGES_LANGUAGES, Router.getInstance().currentRoute);
  });

  test('toggleQuickAnswersUnitConversion', () => {
    let button = page.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#quick-answers-unit-conversion-enable');
    assertEquals(null, button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.unit_conversion.enabled', false);
    flush();

    button =
        page.shadowRoot!.querySelector('#quick-answers-unit-conversion-enable');
    assert(button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.quick_answers.unit_conversion.enabled').value);
  });

  test('Deep link to Preferred Search Engine', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '600');
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE, params);

    const browserSearchSettingsLink =
        page.shadowRoot!.querySelector('settings-search-engine')!.shadowRoot!
            .querySelector('#browserSearchSettingsLink');
    const deepLinkElement =
        browserSearchSettingsLink!.shadowRoot!.querySelector('cr-icon-button');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Preferred Search Engine button should be focused for settingId=600.');
  });

  test('Deep link to Quick Answers On/Off', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '608');
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE, params);

    const deepLinkElement =
        page.shadowRoot!.querySelector('#quick-answers-enable')!.shadowRoot!
            .querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick Answer On/Off toggle should be focused for settingId=608.');
  });

  test('Deep link to Quick Answers Definition', async () => {
    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '609');
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE, params);

    const deepLinkElement =
        page.shadowRoot!.querySelector('#quick-answers-definition-enable')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick Answer definition toggle should be focused for settingId=609.');
  });

  test('Deep link to Quick Answers Translation', async () => {
    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '610');
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE, params);

    const deepLinkElement =
        page.shadowRoot!.querySelector('#quick-answers-translation-enable')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick Answer translation toggle should be focused for settingId=610.');
  });

  test('Deep link to Quick Answers Unit Conversion', async () => {
    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '611');
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE, params);

    const deepLinkElement =
        page.shadowRoot!.querySelector('#quick-answers-unit-conversion-enable')!
            .shadowRoot!.querySelector('cr-toggle');
    assert(deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick Answer unit conversion toggle should be focused for settingId=611.');
  });
});
