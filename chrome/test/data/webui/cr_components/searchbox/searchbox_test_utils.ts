// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AutocompleteResult} from 'chrome://resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';

export function createAutocompleteResult(
    modifiers: Partial<AutocompleteResult> = {}): AutocompleteResult {
  const base: AutocompleteResult = {
    input: '',
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

/**
 * Waits for the specified attribute of a given element to change.
 * @param attributeName The attribute to observe.
 * @param initialValue The value to compare against to detect the attribute
 *     change.
 */
export function waitForAttributeChange(
    element: HTMLElement, attributeName: string, initialValue: string) {
  return new Promise((resolve) => {
    // Create a MutationObserver to watch for attribute changes.
    const observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        if (mutation.type === 'attributes' &&
            mutation.attributeName === attributeName) {
          // Check if the value actually changed.
          const newValue = (element as any)[attributeName];
          if (newValue !== initialValue) {
            observer.disconnect();
            resolve(newValue);
            return;
          }
        }
      }
    });

    // Configure the observer to watch for attribute changes.
    observer.observe(element, {
      attributes: true,
      attributeFilter: [attributeName],
    });
  });
}
