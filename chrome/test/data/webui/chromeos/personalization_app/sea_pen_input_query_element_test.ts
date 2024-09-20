// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {BeginSearchSeaPenThumbnailsAction, SeaPenActionName, SeaPenHistoryPromptSelectedEvent, SeaPenInputQueryElement, SeaPenRecentImageDeleteEvent, SeaPenRouterElement, SeaPenSampleSelectedEvent, SeaPenSuggestionsElement} from 'chrome://personalization/js/personalization_app.js';
import {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {CrTextareaElement} from 'chrome://resources/ash/common/cr_elements/cr_textarea/cr_textarea.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assert} from 'chrome://webui-test/chai.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestPersonalizationStore} from 'test_personalization_store.js';
import {TestSeaPenProvider} from 'test_sea_pen_interface_provider.js';

import {baseSetup, getActiveElement, initElement, teardownElement} from './personalization_app_test_utils.js';

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

  /** Returns the text of the clicked button. */
  async function clickSuggestionButton(): Promise<string> {
    const seaPenSuggestions =
        seaPenInputQueryElement!.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertTrue(!!seaPenSuggestions, 'suggestions should exist');
    const seaPenSuggestionButton =
        seaPenSuggestions.shadowRoot!.querySelector<CrButtonElement>(
            '.suggestion');
    assertTrue(!!seaPenSuggestionButton, 'suggestion buttons should exist');
    const suggestionButtonText = seaPenSuggestionButton.textContent!.trim()!;

    seaPenSuggestionButton.click();
    await waitAfterNextRender(seaPenInputQueryElement as HTMLElement);

    return suggestionButtonText;
  }

  function suggestionExists(suggestion: string): boolean {
    const suggestionArray = getSuggestions();
    return suggestionArray.includes(suggestion);
  }

  async function setTextInputValue(textValue: string) {
    const inputElement =
        seaPenInputQueryElement!.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput')!;
    inputElement.value = textValue;
    await waitAfterNextRender(seaPenInputQueryElement!);
  }

  test('displays recreate button if thumbnails and query exist', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;
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

  test('displays create button when the current query is cleared', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery = null;
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
    let seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(
        !!seaPenSuggestions, 'suggestions element should be hidden on load');

    // Set input text.
    await setTextInputValue('Love looks not with the eyes, but with the mind');

    seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertTrue(
        !!seaPenSuggestions,
        'suggestions element should be show after entering text');

    // Remove input text.
    await setTextInputValue('');

    seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is) as HTMLElement;
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');
  });

  test('disables text input based on thumbnails loading state', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    await setTextInputValue('Love looks not with the eyes, but with the mind');
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');

    assertEquals(
        'true', inputElement?.getAttribute('aria-disabled'),
        'disable text input when thumbnails are loading');

    // Set thumbnails loading completed.
    personalizationStore.data.wallpaper.seaPen = {
        ...personalizationStore.data.wallpaper.seaPen};
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenInputQueryElement);

    assertEquals(
        'false', inputElement?.getAttribute('aria-disabled'),
        'reenable text input when thumbnails finish loading');
  });

  test('displays suggestions based on thumbnails loading state', async () => {
    const textValue = 'Love looks not with the eyes, but with the mind';
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.wallpaper.seaPen.loading.thumbnails = true;

    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);

    await setTextInputValue(textValue);

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
    await setTextInputValue('And therefore is winged Cupid painted blind');

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
    await setTextInputValue('Love looks not with the eyes, but with the mind');
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
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
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
            seaPenInputQueryElement.shadowRoot
                ?.querySelector<CrTextareaElement>('#queryInput');
        assertTrue(!!inputElement, 'textInput should exist');
        await setTextInputValue('Uneasy lies the head that wears the crown.');

        inputElement.focus();

        const seaPenSuggestions =
            seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
                SeaPenSuggestionsElement.is);
        assertTrue(!!seaPenSuggestions, 'suggestions element should be shown');
      });

  test('shuffles suggestions', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    await setTextInputValue('Love looks not with the eyes, but with the mind');
    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    const shuffleButton =
        seaPenSuggestions!.shadowRoot!.getElementById('shuffle');
    assertTrue(!!shuffleButton, 'suggestions button should exist');
    const originalSuggestions = getSuggestions();

    shuffleButton.click();

    await waitAfterNextRender(seaPenInputQueryElement);
    assert.notSameOrderedMembers(originalSuggestions, getSuggestions());
  });

  test('clicking suggestion adds text to whitespace input', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const textValue = '  ';
    await setTextInputValue(textValue);
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');
    assertEquals(textValue, inputElement!.value, 'input should show text');

    const suggestionButtonText = await clickSuggestionButton()!;


    assertEquals(inputElement?.value, suggestionButtonText);
  });

  test('clicking suggestion adds text to end of input', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');
    const textValue = 'Brevity is the soul of wit';
    await setTextInputValue(textValue);

    const suggestionButtonText = await clickSuggestionButton()!;


    assertEquals(
        `${textValue}, ${suggestionButtonText}`, inputElement!.value,
        'suggestion text should be added at the end of the text input');
  });

  test(
      'clicking the third-to-last suggestion resets the suggestion list',
      async () => {
        seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
        await waitAfterNextRender(seaPenInputQueryElement);
        const textValue = 'Brevity is the soul of wit';
        await setTextInputValue(textValue);

        while (getSuggestions().length > 3) {
          await clickSuggestionButton();
        }
        assertEquals(3, getSuggestions().length, 'there are 3 suggestions');

        // Click one more time.
        await clickSuggestionButton();

        assertTrue(getSuggestions().length > 3, 'length of suggestions resets');
      });

  test('clicking suggestion does nothing if over the max length', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');
    // The max length is 1000 characters.
    const textValue = 'a'.repeat(999);
    await setTextInputValue(textValue);

    await clickSuggestionButton();

    assertEquals(
        `${textValue}`, inputElement!.value,
        'suggestion text should not be changed');
  });

  test('clicking suggestion removes the button', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    await setTextInputValue('abc');

    const suggestionButtonText = await clickSuggestionButton();

    assertFalse(
        suggestionExists(suggestionButtonText),
        'clicked suggestion should be removed');
  });

  test('shuffling resets suggestion button list the length is 3', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    await setTextInputValue('abc');
    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is)!;

    // Click all but 3 suggestion buttons.
    while (getSuggestions().length > 3) {
      await clickSuggestionButton();
    }

    const shuffleButton =
        seaPenSuggestions!.shadowRoot!.getElementById('shuffle')!;
    shuffleButton.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    assertTrue(
        getSuggestions().length > 3,
        'suggestion button list should reset if shuffling with very few items');
  });

  test('thumbnails shuffle when they are shown again', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    await setTextInputValue('abc');
    const originalSuggestions = getSuggestions();

    // Clearing text input clears the suggestions.
    await setTextInputValue('');
    assertFalse(
        !!seaPenInputQueryElement!.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is),
        'there should be no suggestions with empty text input');

    // Adding text input adds suggestions again.
    await setTextInputValue('abc');

    // These suggestions should not be the same as the first set of suggestions.
    assert.notSameOrderedMembers(originalSuggestions, getSuggestions());
  });

  test('inspires me', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    personalizationStore.expectAction(
        SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS);
    const inspireButton =
        seaPenInputQueryElement.shadowRoot!.getElementById('inspire');
    assertTrue(!!inspireButton);

    inspireButton!.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');
    assertTrue(!!inputElement?.value, 'input should show text');
    const action = await personalizationStore.waitForAction(
                       SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS) as
        BeginSearchSeaPenThumbnailsAction;
    assertEquals(
        inputElement?.value, action.query.textQuery,
        'search query should match input value');
  });

  test('shows create button after inspire button clicked', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    const inspireButton =
        seaPenInputQueryElement.shadowRoot!.getElementById('inspire');
    assertTrue(!!inspireButton);

    // Shows recreate button.
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));

    inspireButton!.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    // After inspire button is clicked, switch back to the create button.
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
  });

  test('search sample prompt after sample prompt clicked', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const samplePrompt = 'test sample prompt';
    personalizationStore.expectAction(
        SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS);

    // Sample prompt clicked.
    seaPenInputQueryElement.dispatchEvent(
        new SeaPenSampleSelectedEvent(samplePrompt));
    await waitAfterNextRender(seaPenInputQueryElement);

    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');
    assertEquals(
        samplePrompt, inputElement?.value,
        'input element should show sample prompt');
    const action = await personalizationStore.waitForAction(
                       SeaPenActionName.BEGIN_SEARCH_SEA_PEN_THUMBNAILS) as
        BeginSearchSeaPenThumbnailsAction;
    assertEquals(
        samplePrompt, action.query.textQuery,
        'search query should match sample prompt');
  });

  test('shows create button after sample prompt clicked', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');

    // Shows recreate button.
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));

    // Sample prompt clicked.
    seaPenInputQueryElement.dispatchEvent(
        new SeaPenSampleSelectedEvent('test'));
    await waitAfterNextRender(seaPenInputQueryElement);

    // After inspire button is clicked, switch back to the create button.
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
  });

  test('shows history prompt after history prompt clicked', async () => {
    personalizationStore.data.wallpaper.seaPen.thumbnails =
        seaPenProvider.thumbnails;
    personalizationStore.data.wallpaper.seaPen.currentSeaPenQuery =
        seaPenProvider.seaPenFreeformQuery;
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    const icon = searchButton!.querySelector<HTMLElement>('iron-icon');
    const historyPrompt = 'history prompt';

    // Shows recreate button.
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenRecreateButton'),
        searchButton!.innerText);
    assertEquals('personalization-shared:refresh', icon!.getAttribute('icon'));

    // History prompt clicked.
    seaPenInputQueryElement.dispatchEvent(
        new SeaPenHistoryPromptSelectedEvent(historyPrompt));
    await waitAfterNextRender(seaPenInputQueryElement);

    // After history prompt is clicked, switch back to the create button.
    assertEquals(
        seaPenInputQueryElement.i18n('seaPenCreateButton'),
        searchButton!.innerText);
    assertEquals('sea-pen:photo-spark', icon!.getAttribute('icon'));
    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<CrTextareaElement>(
            '#queryInput');
    assertEquals(historyPrompt, inputElement?.value, 'input should show text');
    assertEquals(
        inputElement, getActiveElement(seaPenInputQueryElement),
        'input element should be focused');
  });

  test('rejects HTML query', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    personalizationStore.setReducersEnabled(true);
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    initElement(SeaPenRouterElement, {basePath: '/base'});
    await waitAfterNextRender(seaPenInputQueryElement);
    await setTextInputValue('<div style="blue">');
    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    personalizationStore.expectAction(
        SeaPenActionName.SET_THUMBNAIL_RESPONSE_STATUS_CODE);

    searchButton?.click();
    await waitAfterNextRender(seaPenInputQueryElement);

    await personalizationStore.waitForAction(
        SeaPenActionName.SET_THUMBNAIL_RESPONSE_STATUS_CODE);
    const seaPenSuggestions =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            SeaPenSuggestionsElement.is);
    assertFalse(!!seaPenSuggestions, 'suggestions element should be hidden');
  });

  test('focus on input after a RecentImageDelete event', async () => {
    seaPenInputQueryElement = initElement(SeaPenInputQueryElement);
    await waitAfterNextRender(seaPenInputQueryElement);
    const searchButton =
        seaPenInputQueryElement.shadowRoot!.querySelector<HTMLElement>(
            '#searchButton');
    searchButton?.focus();

    seaPenInputQueryElement.dispatchEvent(new SeaPenRecentImageDeleteEvent());
    await waitAfterNextRender(seaPenInputQueryElement);

    const inputElement =
        seaPenInputQueryElement.shadowRoot?.querySelector<HTMLElement>(
            '#queryInput');
    assertTrue(!!inputElement);
    assertEquals(getActiveElement(seaPenInputQueryElement), inputElement);
  });
});
