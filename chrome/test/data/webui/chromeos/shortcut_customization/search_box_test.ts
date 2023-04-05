// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_box.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {IronDropdownElement} from 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {fakeSearchResults} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {SearchBoxElement} from 'chrome://shortcut-customization/js/search/search_box.js';
import {SearchResultRowElement} from 'chrome://shortcut-customization/js/search/search_result_row.js';
import {setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('searchBoxTest', function() {
  let searchBoxElement: SearchBoxElement|null = null;
  let searchFieldElement: CrToolbarSearchFieldElement|null = null;
  let dropdownElement: IronDropdownElement|null = null;
  let resultsListElement: IronListElement|null = null;

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

    if (searchFieldElement) {
      searchFieldElement.remove();
    }
    searchFieldElement = null;

    if (dropdownElement) {
      dropdownElement.remove();
    }
    dropdownElement = null;

    if (resultsListElement) {
      resultsListElement.remove();
    }
    resultsListElement = null;
  });

  function initSearchBoxElement(): [
    SearchBoxElement, CrToolbarSearchFieldElement, IronDropdownElement,
    IronListElement
  ] {
    const searchBoxElement_ = document.createElement('search-box');
    document.body.appendChild(searchBoxElement_);
    flush();

    const searchFieldElement_ = strictQuery(
        '#search', searchBoxElement_.shadowRoot, CrToolbarSearchFieldElement);
    const dropdownElement_ = strictQuery(
                                 'iron-dropdown', searchBoxElement_.shadowRoot,
                                 HTMLElement) as IronDropdownElement;
    const resultsListElement_ =
        strictQuery('iron-list', searchBoxElement_.shadowRoot, HTMLElement) as
        IronListElement;

    return [
      searchBoxElement_,
      searchFieldElement_,
      dropdownElement_,
      resultsListElement_,
    ];
  }

  async function simulateSearch(query: string) {
    assertTrue(
        !!searchBoxElement,
        'SearchBoxElement was not initialized before simulating search.');
    assertTrue(
        !!searchFieldElement,
        'SearchFieldElement was not initialized before simulating search.');

    // Setting the value of the search field searches for the query after a
    // short period of time.
    searchFieldElement.setValue(query);

    if (query) {
      await waitForSearchResultsFetched();
      await waitForListUpdate();
    }
    flush();
  }

  async function waitForSearchResultsFetched() {
    assertTrue(!!searchBoxElement);
    await eventToPromise('search-results-fetched', searchBoxElement);
    flush();
  }

  async function waitForListUpdate() {
    assertTrue(!!resultsListElement);
    // Wait for iron-list to complete resizing.
    await eventToPromise('iron-resize', resultsListElement);
    flush();
  }

  function isSearchTextSelected() {
    assertTrue(!!searchFieldElement);
    const input = searchFieldElement.getSearchInput();
    return input.selectionStart === 0 &&
        input.selectionEnd === input.value.length;
  }

  test('SearchBoxLoaded', async () => {
    [searchBoxElement] = initSearchBoxElement();
    assertTrue(!!searchBoxElement);
  });

  test('SearchResultsPopulated', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    // Before: No search results shown.
    assertEquals('', searchFieldElement.getValue());
    assertFalse(searchBoxElement.shouldShowDropdown);
    assertFalse(dropdownElement.opened);
    assertEquals(0, searchBoxElement.searchResults.length);

    await simulateSearch('query');

    // After: Fake search results shown.
    assertEquals('query', searchFieldElement.getValue());
    assertTrue(searchBoxElement.shouldShowDropdown);
    assertTrue(dropdownElement.opened);
    assertEquals(3, searchBoxElement.searchResults.length);

    // Click outside and the dropdown should be closed.
    searchBoxElement.dispatchEvent(new Event('blur'));
    assertFalse(searchBoxElement.shouldShowDropdown);
    assertFalse(dropdownElement.opened);
  });

  test('RestorePreviousExistingResults', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    await simulateSearch('query');

    assertEquals(3, searchBoxElement.searchResults.length);
    assertTrue(
        !!resultsListElement.items, 'iron-list element should have items.');
    assertEquals(3, resultsListElement.items.length);
    assertTrue(dropdownElement.opened);

    // Save the results for later.
    const [firstResult, secondResult, thirdResult] = resultsListElement.items;

    // User clicks outside the search box, closing the dropdown.
    searchBoxElement.blur();
    assertFalse(dropdownElement.opened, 'Expected dropdown to be closed.');

    // User clicks on input, restoring old results and opening dropdown.
    searchFieldElement.getSearchInput().focus();

    assertEquals('query', searchFieldElement.getValue());
    assertTrue(
        dropdownElement.opened,
        'Expected dropdown to be opened after focusing on the search field.');

    // The same result rows exist.
    assertEquals(firstResult, resultsListElement.items[0]);
    assertEquals(secondResult, resultsListElement.items[1]);
    assertEquals(thirdResult, resultsListElement.items[2]);

    // Search field is blurred, closing the dropdown.
    searchFieldElement.getSearchInput().blur();
    assertFalse(dropdownElement.opened, 'Expected dropdown to be closed.');

    // User clicks on input, restoring old results and opening dropdown.
    searchFieldElement.getSearchInput().focus();
    assertEquals('query', searchFieldElement.getValue());
    assertTrue(
        dropdownElement.opened,
        'Expected dropdown to be opened after focusing on the search field.');

    // The same result rows exist.
    assertEquals(firstResult, resultsListElement.items[0]);
    assertEquals(secondResult, resultsListElement.items[1]);
    assertEquals(thirdResult, resultsListElement.items[2]);
  });

  test('ArrowKeysChangeSelectedItem', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    await simulateSearch('query');

    assertEquals(3, searchBoxElement.searchResults.length);
    assertTrue(
        !!resultsListElement.items, 'iron-list element should have items.');
    assertEquals(3, resultsListElement.items.length);
    assertTrue(dropdownElement.opened);

    // The first row should be selected when results are fetched.
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[0]);

    // Test ArrowUp and ArrowDown interaction with selecting.
    const arrowUpEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowUp', keyCode: 38});
    const arrowDownEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowDown', keyCode: 40});

    // ArrowDown event should select next row.
    searchBoxElement.dispatchEvent(arrowDownEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[1]);

    // Move down to the last row
    searchBoxElement.dispatchEvent(arrowDownEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[2]);

    // If last row selected, ArrowDown brings select back to first row.
    searchBoxElement.dispatchEvent(arrowDownEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[0]);

    // If first row selected, ArrowUp brings select back to last row.
    searchBoxElement.dispatchEvent(arrowUpEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[2]);

    // ArrowUp should bring select previous row.
    searchBoxElement.dispatchEvent(arrowUpEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[1]);

    // Test that ArrowLeft and ArrowRight do nothing.
    const arrowLeftEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});
    const arrowRightEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});

    // No change on ArrowLeft
    searchBoxElement.dispatchEvent(arrowLeftEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[1]);

    // No change on ArrowRight
    searchBoxElement.dispatchEvent(arrowRightEvent);
    assertEquals(resultsListElement.selectedItem, resultsListElement.items[1]);
  });

  test('ClickingMagnifyingGlassOpensDropdownAndSelectsText', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    await simulateSearch('query');
    assertTrue(dropdownElement.opened);

    // Click away from the search box.
    searchBoxElement.blur();
    assertFalse(dropdownElement.opened);
    assertFalse(isSearchTextSelected());

    // Click the search icon and the text should be selected.
    strictQuery(
        'cr-icon-button#icon', searchFieldElement.shadowRoot, HTMLElement)
        .click();
    assertTrue(isSearchTextSelected());
    assertTrue(dropdownElement.opened);
  });

  test('Keypress Enter on selected row causes dropdown to close', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    await simulateSearch('query');
    assertTrue(dropdownElement.opened);
    assertTrue(searchBoxElement.shouldShowDropdown);

    const selectedRow = strictQuery(
        'search-result-row[selected]', dropdownElement, SearchResultRowElement);
    const selectedRowInnerElement = strictQuery(
        '#searchResultRowInner', selectedRow.shadowRoot, HTMLDivElement);

    const enterEvent = new KeyboardEvent(
        'keypress', {cancelable: true, key: 'Enter', keyCode: 13});
    selectedRowInnerElement.dispatchEvent(enterEvent);
    assertFalse(dropdownElement.opened);
  });

  test('Clicking on a row closes the dropdown', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    await simulateSearch('query');
    assertTrue(dropdownElement.opened);
    assertTrue(searchBoxElement.shouldShowDropdown);

    const selectedRow = strictQuery(
        'search-result-row[selected]', dropdownElement, SearchResultRowElement);
    const selectedRowInnerElement = strictQuery(
        '#searchResultRowInner', selectedRow.shadowRoot, HTMLDivElement);

    selectedRowInnerElement.click();
    assertFalse(dropdownElement.opened);
  });
});
