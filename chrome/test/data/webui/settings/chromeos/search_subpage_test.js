// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SearchSubpage', function() {
  /** @type {SearchSubpageElement} */
  let page = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      shouldShowQuickAnswersSettings: true,
      quickAnswersSubToggleEnabled: true,
    });
  });

  setup(function() {
    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      page = document.createElement('settings-search-subpage');
      page.prefs = prefElement.prefs;
      document.body.appendChild(page);
      flush();
    });
  });

  teardown(function() {
    page.remove();
    CrSettingsPrefs.resetForTesting();
  });

  test('definitionToggleVisibility', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-definition-enable');
    assertFalse(!!button);

    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    button = page.shadowRoot.querySelector('#quick-answers-definition-enable');
    assertTrue(!!button);
  });

  test('translationToggleVisibility', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-translation-enable');
    assertFalse(!!button);

    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    button = page.shadowRoot.querySelector('#quick-answers-translation-enable');
    assertTrue(!!button);
  });

  test('unitConversionToggleVisibility', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable');
    assertFalse(!!button);

    page.setPrefValue('settings.quick_answers.enabled', true);
    flush();

    button =
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable');
    assertTrue(!!button);
  });

  test('toggleQuickAnswers', function() {
    flush();
    const button = page.shadowRoot.querySelector('#quick-answers-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    let definition_button =
        page.shadowRoot.querySelector('#quick-answers-definition-enable');
    let translation_button =
        page.shadowRoot.querySelector('#quick-answers-translation-enable');
    let unit_conversion_button =
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable');
    assertFalse(!!definition_button);
    assertFalse(!!translation_button);
    assertFalse(!!unit_conversion_button);

    // Tap the enable toggle button and ensure the state becomes enabled.
    button.click();
    flush();
    assertTrue(button.checked);

    definition_button =
        page.shadowRoot.querySelector('#quick-answers-definition-enable');
    translation_button =
        page.shadowRoot.querySelector('#quick-answers-translation-enable');
    unit_conversion_button =
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable');
    assertTrue(!!definition_button);
    assertTrue(!!translation_button);
    assertTrue(!!unit_conversion_button);
  });

  test('toggleQuickAnswersDefinition', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-definition-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.definition.enabled', false);
    flush();

    button = page.shadowRoot.querySelector('#quick-answers-definition-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(page.getPref('settings.quick_answers.definition.enabled.value'));
  });

  test('toggleQuickAnswersTranslation', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-translation-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.translation.enabled', false);
    flush();

    button = page.shadowRoot.querySelector('#quick-answers-translation-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.quick_answers.translation.enabled.value'));
  });

  test('clickLanguageSettingsLink', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-translation-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.translation.enabled', false);
    flush();

    button = page.shadowRoot.querySelector('#quick-answers-translation-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    const languageSettingsLink =
        button.shadowRoot.querySelector('#sub-label-text-with-link')
            .querySelector('a');
    assertTrue(!!languageSettingsLink);

    languageSettingsLink.click();
    flush();
    assertFalse(button.checked);
    assertFalse(
        page.getPref('settings.quick_answers.translation.enabled.value'));

    assertEquals(
        routes.OS_LANGUAGES_LANGUAGES, Router.getInstance().getCurrentRoute());
  });

  test('toggleQuickAnswersUnitConversion', function() {
    let button =
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable');
    assertFalse(!!button);
    page.setPrefValue('settings.quick_answers.enabled', true);
    page.setPrefValue('settings.quick_answers.unit_conversion.enabled', false);
    flush();

    button =
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.quick_answers.unit_conversion.enabled.value'));
  });

  test('Deep link to Preferred Search Engine', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '600');
    Router.getInstance().navigateTo(routes.SEARCH_SUBPAGE, params);

    const browserSearchSettingsLink =
        page.shadowRoot.querySelector('settings-search-engine')
            .shadowRoot.querySelector('#browserSearchSettingsLink');
    const deepLinkElement =
        browserSearchSettingsLink.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!deepLinkElement);
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
        page.shadowRoot.querySelector('#quick-answers-enable')
            .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
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
        page.shadowRoot.querySelector('#quick-answers-definition-enable')
            .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
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
        page.shadowRoot.querySelector('#quick-answers-translation-enable')
            .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
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
        page.shadowRoot.querySelector('#quick-answers-unit-conversion-enable')
            .shadowRoot.querySelector('cr-toggle');
    assertTrue(!!deepLinkElement);
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Quick Answer unit conversion toggle should be focused for settingId=611.');
  });
});
