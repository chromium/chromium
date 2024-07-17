// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {SeaPenInputQueryElement, SeaPenRouterElement, SeaPenSuggestionsElement, setSeaPenThumbnailsAction} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
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

  function getSuggestions(): string[] {
    const seaPenSuggestions =
        seaPenInputQueryElement!.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    const suggestions =
        seaPenSuggestions!.shadowRoot!.querySelectorAll<HTMLElement>(
            '.suggestion');
    assertTrue(!!suggestions, 'suggestions should exist');

    return Array.from(suggestions).map(suggestion => {
      assertTrue(!!suggestion.textContent);
      return suggestion.textContent.trim();
    });
  }

  function suggestionExists(suggestion: string): boolean {
    const suggestionArray = getSuggestions();
    return suggestionArray.includes(suggestion);
  }

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
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');
  });

  test('displays suggestions based on thumbnails loading state', async () => {
    const textValue = 'Love looks not with the eyes, but with the mind';
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;

    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');

    // Set input text.
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);

    // Suggestions should not display as thumbnails are loading.
    let seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');

    // Set thumbnails loading completed and update the current Sea Pen query
    // to match with the textValue.
    personalizationStore.data.wallpaper.seaPen = {
        ...personalizationStore.data.wallpaper.seaPen};
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery = {
      textQuery: textValue,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenInputQueryElement);

    // No changes in suggestions display state as the current Sea Pen query is
    // same as the text input.
    seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');

    // Update the text input to be different from the current query.
    inputElement.value = 'And therefore is winged Cupid painted blind';
    await waitAfterNextRender(seaPenInputQueryElement);

    // Sea Pen suggestions should display now.
    seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertTrue(!!seaPenSuggestions, 'suggestions element should be shown');
  });

  test('hide suggestions after clicking create', async () => {
    personalizationStore.setReducersEnabled(true);
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    initElement(SeaPenRouterElement, {basePath: '/base'});
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

  test('focusing on empty text input does not show suggestions', async () => {
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
      'focusing on non-empty text input shows suggestions', async () => {
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

  test('shuffles suggestions', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');

    // Set input text.
    inputElement.value = 'Love looks not with the eyes, but with the mind';
    await waitAfterNextRender(seaPenInputQueryElement);

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);

    const shuffleButton =
        seaPenSuggestions!.shadowRoot!.getElementById('shuffle');

    assertTrue(!!shuffleButton, 'suggestions button should exist');

    const originalSuggestions = getSuggestions();
    shuffleButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);
    chai.assert.notSameOrderedMembers(originalSuggestions, getSuggestions());
    chai.assert.sameMembers(originalSuggestions, getSuggestions());
  });


  test('displays prompting guide link', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const promptingGuide =
        seaPenInputQueryElement.shadowRoot!.getElementById('promptingGuide');

    assertTrue(!!promptingGuide, 'prompting guide link should exist');
  });

  test('clicking suggestion adds text to whitespace input', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = '  ';
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);
    assertEquals(textValue, inputElement.value, 'input should show text');

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertTrue(!!seaPenSuggestions);
    const seaPenSuggestionButton =
        seaPenSuggestions.shadowRoot!.querySelector<CrButtonElement>(
            '.suggestion');
    assertTrue(!!seaPenSuggestionButton);
    const suggestionButtonText = seaPenSuggestionButton.textContent?.trim();

    seaPenSuggestionButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    assertEquals(inputElement?.value, suggestionButtonText);
  });

  test('clicking suggestion adds text to end of input', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = 'Brevity is the soul of wit';
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement, 'textInput should exist');
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);
    assertEquals(textValue, inputElement.value, 'input should show text');

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertTrue(!!seaPenSuggestions, 'suggestions should exist');
    const seaPenSuggestionButton =
        seaPenSuggestions.shadowRoot!.querySelector<CrButtonElement>(
            '.suggestion');
    assertTrue(!!seaPenSuggestionButton, 'suggestion buttons should exist');
    const suggestionButtonText = seaPenSuggestionButton.textContent?.trim();

    seaPenSuggestionButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    assertEquals(
        `${textValue}, ${suggestionButtonText}`, inputElement.value,
        'suggestion text should be added at the end of the text input');
  });

  test('clicking suggestion removes the button', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = '  ';
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput')!;
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is)!;
    const seaPenSuggestionButton =
        seaPenSuggestions.shadowRoot!.querySelector<CrButtonElement>(
            '.suggestion')!;
    const suggestionButtonText = seaPenSuggestionButton.textContent?.trim()!;
    const originalLength = getSuggestions().length;

    seaPenSuggestionButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    assertEquals(
        originalLength - 1, getSuggestions().length,
        'suggestion list should be shorter');
    assertFalse(
        suggestionExists(suggestionButtonText),
        'clicked suggestion should be removed');
  });

  test('shuffling only removes recently clicked suggestions', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = '  ';
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput')!;
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is)!;
    let suggestionButtons =
        seaPenSuggestions.shadowRoot!.querySelectorAll<CrButtonElement>(
            '.suggestion')!;
    const firstSuggestionButton = suggestionButtons?.[0];
    const firstSuggestionButtonText =
        firstSuggestionButton?.textContent?.trim()!;
    const shuffleButton =
        seaPenSuggestions!.shadowRoot!.getElementById('shuffle')!;

    firstSuggestionButton?.click();
    shuffleButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    // The first shuffle should remove the firstSeaPenSuggestionButton.
    assertFalse(
        suggestionExists(firstSuggestionButtonText),
        'the first suggestion should be removed for one shuffle');

    suggestionButtons =
        seaPenSuggestions.shadowRoot!.querySelectorAll<CrButtonElement>(
            '.suggestion')!;
    const secondSuggestionButton = suggestionButtons?.[0];
    const secondSuggestionButtonText =
        firstSuggestionButton?.textContent?.trim()!;

    secondSuggestionButton?.click();
    shuffleButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    // The second shuffle should remove only the
    // secondSeaPenSuggestionButton because it was clicked since the last
    // shuffle.
    assertTrue(
        suggestionExists(firstSuggestionButtonText),
        'first clicked suggestion should return to the list');
    assertFalse(
        suggestionExists(secondSuggestionButtonText),
        'second clicked suggestion should now be removed from the list');
  });

  test('reset suggestion button list if the length is 3 or less', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = '  ';
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput')!;
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);

    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is)!;
    let suggestionButtons =
        seaPenSuggestions.shadowRoot!.querySelectorAll<CrButtonElement>(
            '.suggestion')!;
    const originalLength = suggestionButtons.length;

    // Click all but 3 suggestion buttons.
    while (getSuggestions().length > 3) {
      const suggestionButton =
          seaPenSuggestions.shadowRoot!.querySelector<CrButtonElement>(
              '.suggestion')!;
      suggestionButton.click();
      await waitAfterNextRender(seaPenInputQueryElement);
    }

    const shuffleButton =
        seaPenSuggestions!.shadowRoot!.getElementById('shuffle')!;
    shuffleButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    suggestionButtons =
        seaPenSuggestions.shadowRoot!.querySelectorAll<CrButtonElement>(
            '.suggestion')!;
    assertEquals(
        originalLength, suggestionButtons.length,
        'suggestion button list should reset if shuffling with very few items');
  });

  test('thumbnail updates reset the suggestion list', async () => {
    personalizationStore.setReducersEnabled(true);
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = '  ';
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput')!;
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement);
    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is)!;
    const seaPenSuggestionButton =
        seaPenSuggestions.shadowRoot!.querySelector<CrButtonElement>(
            '.suggestion')!;
    const suggestionButtonText = seaPenSuggestionButton.textContent?.trim()!;
    const originalLength = getSuggestions().length;

    seaPenSuggestionButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    assertEquals(
        originalLength - 1, getSuggestions().length,
        'after clicking a suggestion button, the suggestion should be removed');
    assertFalse(
        suggestionExists(suggestionButtonText),
        'the clicked suggestion should not be in the list');

    // Simulate a query was run and we got the thumbnails, one of the sea pen
    // thumbnails was selected.
    personalizationStore.dispatch(setSeaPenThumbnailsAction(
        seaPenProvider.seaPenQuery, seaPenProvider.thumbnails));
    await waitAfterNextRender(seaPenInputQueryElement);

    assertEquals(
        originalLength, getSuggestions().length,
        'after receiving thumbnails, the suggestion list should be reset');
    assertTrue(
        suggestionExists(suggestionButtonText),
        'after receiving thumbnails, the suggestion button should appear');
  });

  test('inspires me', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    const inspireButton =
        seaPenInputQueryElement.shadowRoot!.getElementById('inspire');
    assertTrue(!!inspireButton);
    inspireButton!.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrInputElement>(
            '#queryInput');
    assertTrue(!!inputElement?.value, 'input should show text');
  });
});
