// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_box.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {IronDropdownElement} from '//resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {fakeSearchResults} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {SearchBoxElement} from 'chrome://shortcut-customization/js/search/search_box.js';
import {setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

function initSearchBoxElement(): SearchBoxElement {
  const element = document.createElement('search-box');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('searchBoxTest', function() {
  let searchBoxElement: SearchBoxElement|null = null;

  let handler: FakeShortcutSearchHandler;

  setup(() => {
    // Set up SearchHandler.
    handler = new FakeShortcutSearchHandler();
    handler.setFakeSearchResult(fakeSearchResults);
    setShortcutSearchHandlerForTesting(handler);
  });

  teardown(() => {
    if (searchBoxElement) {
      searchBoxElement.remove();
    }
    searchBoxElement = null;
  });

  test('SearchBoxLoaded', async () => {
    searchBoxElement = initSearchBoxElement();
    await flush();
    assertTrue(!!searchBoxElement);
  });

  test('SearchResultsPopulated', async () => {
    searchBoxElement = initSearchBoxElement();
    await flush();

    const searchFieldElement = strictQuery(
        '#search', searchBoxElement.shadowRoot, CrToolbarSearchFieldElement);
    const dropdownElement = strictQuery(
                                'iron-dropdown', searchBoxElement.shadowRoot,
                                HTMLElement) as IronDropdownElement;

    // Before: No search results shown.
    assertEquals('', searchFieldElement.textContent);
    assertFalse(searchBoxElement.shouldShowDropdown);
    assertFalse(dropdownElement.opened);
    assertEquals(0, searchBoxElement.searchResults.length);

    // Press enter will invoke shortcut search.
    searchFieldElement.textContent = 'query';
    searchFieldElement.dispatchEvent(
        new KeyboardEvent('keydown', {'key': 'Enter'}));

    await flush();

    // After: Fake search results shown.
    assertEquals('query', searchFieldElement.textContent);
    assertTrue(searchBoxElement.shouldShowDropdown);
    assertTrue(dropdownElement.opened);
    assertEquals(3, searchBoxElement.searchResults.length);

    // Click outside and the dropdown should be closed.
    searchBoxElement.dispatchEvent(new Event('blur'));
    assertFalse(searchBoxElement.shouldShowDropdown);
    assertFalse(dropdownElement.opened);
  });
});
