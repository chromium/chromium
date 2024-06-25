// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenInputQueryElement, SeaPenSuggestionsElement} from 'chrome://personalization/js/personalization_app.js';
import {CrInputElement} from 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestPersonalizationStore} from 'test_personalization_store.js';
import {TestSeaPenProvider} from 'test_sea_pen_interface_provider.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';

suite('SeaPenInputQueryElementTest', function() {
  let seaPenInputQueryElement: SeaPenInputQueryElement|null;
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;

  setup(function() {
    loadTimeData.overrideValues({isSeaPenTextInputEnabled: true});
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenInputQueryElement);
    seaPenInputQueryElement = null;
  });

  test('displays recreate button if thumbnails exist', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');

    assertEquals(
        seaPenInputQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));
  });

  test('displays create button when no thumbnails are generated', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails = null;
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');

    assertEquals(
        seaPenInputQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
  });

  test('displays suggestions when text input is entered', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');

    let seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(
        !!seaPenSuggestions, 'suggestions element should be hidden on load');

    // Set input text.
    inputElement.value = 'Love looks not with the eyes, but with the mind';
    await waitAfterNextRender(seaPenInputQueryElement);

    seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertTrue(
        !!seaPenSuggestions,
        'suggestions element should be show after entering text');

    // Remove input text.
    inputElement.value = '';
    await waitAfterNextRender(seaPenInputQueryElement);

    seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is) as HTMLElement;
    assertEquals(
        'none', getComputedStyle(seaPenSuggestions).display,
        'suggestions element should be hidden after hiding text');
  });

  test('hide suggestions after clicking create', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');
    inputElement.value = 'Love looks not with the eyes, but with the mind';
    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');

    searchButton?.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');
  });

  test('focusing does not show suggestions', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');

    inputElement.focus();

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');
  });

  test(
      'focusing shows suggestions if there are thumbnails and text',
      async () => {
        personalizationStore.data.wallpaper.seaPen.thumbnails =
            seaPenProvider.thumbnails;
        seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
        await waitAfterNextRender(seaPenInputQueryElement);
        const inputElement =
            seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
                '#queryInput');
        assertTrue(!!inputElement, 'textInput should exist');
        // Set input text.
        inputElement.value = 'Uneasy lies the head that wears the crown.';
        await waitAfterNextRender(seaPenInputQueryElement);

        inputElement.focus();

        const seaPenSuggestions =
            seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
                SeaPenSuggestionsElement.is);
        assertTrue(!!seaPenSuggestions, 'suggestions element should be shown');
      });

  test('displays prompting guide link', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const promptingGuide =
        seaPenInputQueryElement.shadowRoot!.getElementById('promptingGuide');

    assertTrue(!!promptingGuide, 'prompting guide link should exist');
  });
});
