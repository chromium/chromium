// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import 'chrome://new-tab-page/strings.m.js';

import {createAutocompleteResultForTesting, createSearchMatchForTesting} from 'chrome://resources/cr_components/searchbox/searchbox_browser_proxy.js';
import type {SearchboxDropdownElement} from 'chrome://resources/cr_components/searchbox/searchbox_dropdown.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {$$, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createSuggestTemplateInfo} from './searchbox_test_utils.js';

// TODO(crbug.com/455876602): Move dropdown-specific tests in searchbox_test.ts
//  into this file.
suite('SearchboxDropdown', () => {
  let dropdown: SearchboxDropdownElement;

  setup(() => {
    loadTimeData.overrideValues({askGSuggestionSidePanelDescription: false});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('cr-searchbox-dropdown');
    document.body.appendChild(dropdown);
  });

  test('hides matches that have `isHidden` field set to true', async () => {
    // Arrange.
    const matches = [
      createSearchMatchForTesting({
        suggestTemplate: createSuggestTemplateInfo({primaryText: 'bar'}),
        isHidden: true,
      }),
      createSearchMatchForTesting({
        suggestTemplate: createSuggestTemplateInfo({primaryText: 'foo'}),
      }),
    ];

    // Act.
    dropdown.result = createAutocompleteResultForTesting({matches});
    await microtasksFinished();

    // Assert.
    const matchEls = dropdown.shadowRoot.querySelectorAll('cr-searchbox-match');
    assertEquals(1, matchEls.length);
    const primaryTextEl = $$(matchEls[0]!, '#primaryText');
    assertTrue(!!primaryTextEl);
    assertEquals('foo', primaryTextEl.textContent.trim());
    // The visible element's matchIndex must retain its original index (1),
    // even though it is the first (and only) visible element.
    assertEquals(1, matchEls[0]!.matchIndex);
  });

  test(
      'hides the AskG entrypoint description while a contextual suggestion ' +
          'with the same description is selected',
      async () => {
        // Arrange.
        const [entrypointEl, contextualEl, otherEl] =
            await createDescriptionDedupeDropdown(/*dedupe=*/ true);
        assertTrue(entrypointEl!.hasAttribute('duplicate-description'));
        assertTrue(contextualEl!.hasAttribute('duplicate-description'));
        assertFalse(otherEl!.hasAttribute('duplicate-description'));
        assertEquals('visible', descriptionVisibility(entrypointEl!));

        // Act & Assert.
        await dropdown.selectIndex(0);
        assertEquals('visible', descriptionVisibility(entrypointEl!));

        await dropdown.selectIndex(1);
        assertEquals('hidden', descriptionVisibility(entrypointEl!));

        await dropdown.selectIndex(2);
        assertEquals('visible', descriptionVisibility(entrypointEl!));
      });

  test('does not hide descriptions without the param', async () => {
    const [entrypointEl, contextualEl] =
        await createDescriptionDedupeDropdown(/*dedupe=*/ false);
    assertFalse(entrypointEl!.hasAttribute('duplicate-description'));
    assertFalse(contextualEl!.hasAttribute('duplicate-description'));

    await dropdown.selectIndex(1);
    assertEquals('visible', descriptionVisibility(entrypointEl!));
  });

  function descriptionVisibility(matchEl: HTMLElement): string {
    return getComputedStyle($$(matchEl, '#secondaryText')!).visibility;
  }

  // Creates a dropdown with an AskG entrypoint-like match and a contextual
  // suggestion sharing its description, plus a contextual suggestion with a
  // different description.
  async function createDescriptionDedupeDropdown(dedupe: boolean) {
    loadTimeData.overrideValues({askGSuggestionSidePanelDescription: dedupe});
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    dropdown = document.createElement('cr-searchbox-dropdown');
    document.body.appendChild(dropdown);

    const withDescription = (secondaryText: string) =>
        createSuggestTemplateInfo({secondaryText});
    dropdown.result = createAutocompleteResultForTesting({
      matches: [
        createSearchMatchForTesting(
            {suggestTemplate: withDescription('Opens in side panel')}),
        createSearchMatchForTesting({
          showSecondaryTextOnlyOnSelection: true,
          suggestTemplate: withDescription('Opens in side panel'),
        }),
        createSearchMatchForTesting({
          showSecondaryTextOnlyOnSelection: true,
          suggestTemplate: withDescription('Ask Google about this page'),
        }),
      ],
    });
    await microtasksFinished();
    return dropdown.shadowRoot.querySelectorAll('cr-searchbox-match');
  }
});
