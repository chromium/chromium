// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {stringToMojoString16} from 'chrome://resources/js/mojo_type_util.js';
import type {AutocompleteResult} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

export function createAutocompleteResult(
    modifiers: Partial<AutocompleteResult> = {}): AutocompleteResult {
  const base: AutocompleteResult = {
    input: stringToMojoString16(''),
    matches: [],
    suggestionGroupsMap: {},
    smartComposeInlineHint: null,
  };

  return Object.assign(base, modifiers);
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
