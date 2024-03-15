// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://history/strings.m.js';
import 'chrome://resources/cr_components/history_embeddings/filter_chips.js';

import type {HistoryEmbeddingsFilterChips} from 'chrome://resources/cr_components/history_embeddings/filter_chips.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('cr-history-embeddings-filter-chips', () => {
  let element: HistoryEmbeddingsFilterChips;

  setup(() => {
    loadTimeData.overrideValues({
      historyEmbeddingsSuggestion1: 'suggestion 1',
      historyEmbeddingsSuggestion2: 'suggestion 2',
      historyEmbeddingsSuggestion3: 'suggestion 3',
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    element = document.createElement('cr-history-embeddings-filter-chips');
    document.body.appendChild(element);
    return flushTasks();
  });

  test('UpdatesByGroupChipByBinding', () => {
    assertFalse(element.$.byGroupChip.hasAttribute('selected'));
    assertEquals('history-embeddings:by-group', element.$.byGroupChipIcon.icon);
    element.showResultsByGroup = true;
    assertTrue(element.$.byGroupChip.hasAttribute('selected'));
    assertEquals('cr:check', element.$.byGroupChipIcon.icon);
  });

  test('UpdatesByGroupChipByClicking', async () => {
    let notifyEventPromise =
        eventToPromise('show-results-by-group-changed', element);
    element.$.byGroupChip.click();
    let notifyEvent = await notifyEventPromise;
    assertTrue(element.showResultsByGroup);
    assertTrue(element.$.byGroupChip.hasAttribute('selected'));
    assertTrue(notifyEvent.detail.value);

    notifyEventPromise =
        eventToPromise('show-results-by-group-changed', element);
    element.$.byGroupChip.click();
    notifyEvent = await notifyEventPromise;
    assertFalse(element.showResultsByGroup);
    assertFalse(element.$.byGroupChip.hasAttribute('selected'));
    assertFalse(notifyEvent.detail.value);
  });

  test('UpdatesSuggestionsOnBinding', () => {
    function getSelectedChips() {
      return element.shadowRoot!.querySelectorAll(
          '#suggestions cr-chip[selected]');
    }
    assertEquals(0, getSelectedChips().length);

    ['suggestion 1', 'suggestion 2', 'suggestion 3'].forEach(suggestion => {
      // Update the search query binding with each suggestion's text should
      // mark that suggestion as selected.
      element.selectedSuggestion = suggestion;
      const selectedChips = getSelectedChips();
      assertEquals(1, selectedChips.length);
      assertEquals(suggestion, selectedChips[0]!.textContent!.trim());
    });

    element.selectedSuggestion = undefined;
    assertEquals(0, getSelectedChips().length);
  });

  test('SelectingSuggestionsDispatchesEvents', async () => {
    async function clickChipAndGetSelectedSuggestion(chip: HTMLElement):
        Promise<string> {
      const eventPromise =
          eventToPromise('selected-suggestion-changed', element);
      chip.click();
      const event = await eventPromise;
      return event.detail.value;
    }

    const suggestions = element.shadowRoot!.querySelectorAll<HTMLElement>(
        '#suggestions cr-chip');
    assertEquals(
        'suggestion 1',
        await clickChipAndGetSelectedSuggestion(suggestions[0]!));
    assertEquals(
        'suggestion 2',
        await clickChipAndGetSelectedSuggestion(suggestions[1]!));
    assertEquals(
        'suggestion 3',
        await clickChipAndGetSelectedSuggestion(suggestions[2]!));
  });

  test('UnselectingSuggestionsDispatchesEvent', async () => {
    element.selectedSuggestion = 'suggestion 1';
    const selectedChip = element.shadowRoot!.querySelector<HTMLElement>(
        '#suggestions cr-chip[selected]');
    assertTrue(!!selectedChip);
    assertEquals('suggestion 1', selectedChip.textContent!.trim());

    const eventPromise = eventToPromise('selected-suggestion-changed', element);
    selectedChip.click();
    const event = await eventPromise;
    assertEquals(undefined, event.detail.value);
  });
});
