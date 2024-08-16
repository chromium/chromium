// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AutocompleteMatch} from 'chrome://resources/cr_components/searchbox/searchbox.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

export function createAutocompleteMatch(): AutocompleteMatch {
  return {
    a11yLabel: {data: []},
    actions: [],
    allowedToBeDefaultMatch: false,
    isSearchType: false,
    swapContentsAndDescription: false,
    supportsDeletion: false,
    suggestionGroupId: -1,  // Indicates a missing suggestion group Id.
    contents: {data: []},
    contentsClass: [{offset: 0, style: 0}],
    description: {data: []},
    descriptionClass: [{offset: 0, style: 0}],
    destinationUrl: {url: ''},
    inlineAutocompletion: {data: []},
    fillIntoEdit: {data: []},
    iconUrl: '',
    imageDominantColor: '',
    imageUrl: '',
    removeButtonA11yLabel: {data: []},
    type: '',
    isRichSuggestion: false,
    isWeatherAnswerSuggestion: null,
    answer: null,
    tailSuggestCommonPrefix: null,
  };
}

/**
 * Asserts the computed style value for an element.
 * @param name The name of the style to assert.
 * @param expected The expected style value.
 */
export function assertStyle(element: Element, name: string, expected: string) {
  const actual = window.getComputedStyle(element).getPropertyValue(name).trim();
  assertEquals(expected, actual);
}
