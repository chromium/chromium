// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_box.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {getDeepActiveElement} from 'chrome://resources/ash/common/util.js';
import {CrToolbarSearchFieldElement} from 'chrome://resources/ash/common/cr_elements/cr_toolbar/cr_toolbar_search_field.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {IronDropdownElement} from 'chrome://resources/polymer/v3_0/iron-dropdown/iron-dropdown.js';
import {IronListElement} from 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {CycleTabsTextSearchResult, fakeAcceleratorConfig, fakeLayoutInfo, fakeSearchResults, TakeScreenshotSearchResult} from 'chrome://shortcut-customization/js/fake_data.js';
import {FakeShortcutSearchHandler} from 'chrome://shortcut-customization/js/search/fake_shortcut_search_handler.js';
import {SearchBoxElement} from 'chrome://shortcut-customization/js/search/search_box.js';
import {SearchResultRowElement} from 'chrome://shortcut-customization/js/search/search_result_row.js';
import {setShortcutSearchHandlerForTesting} from 'chrome://shortcut-customization/js/search/shortcut_search_handler.js';
import {AcceleratorState, MojoSearchResult} from 'chrome://shortcut-customization/js/shortcut_types.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('searchBoxTest', function() {
  let searchBoxElement: SearchBoxElement|null = null;
  let searchFieldElement: CrToolbarSearchFieldElement|null = null;
  let dropdownElement: IronDropdownElement|null = null;
  let resultsListElement: IronListElement|null = null;

  let handler: FakeShortcutSearchHandler;
  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    // Set up SearchHandler.
    handler = new FakeShortcutSearchHandler();
    handler.setFakeSearchResult(fakeSearchResults);
    setShortcutSearchHandlerForTesting(handler);

    // Set up manager.
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
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
    searchFieldElement.$.searchInput.focus();
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

  test('Focus search input on open', async () => {
    [searchBoxElement, searchFieldElement] = initSearchBoxElement();

    waitAfterNextRender(searchBoxElement);
    await flushTasks();

    // The search input should be focused after the first render.
    assertEquals(searchFieldElement.getSearchInput(), getDeepActiveElement());
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

  test('Search availability changed', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    await simulateSearch('query');

    assertTrue(dropdownElement.opened);
    assertEquals(3, searchBoxElement.searchResults.length);

    // Add a new search result, but don't trigger the observer callback.
    handler.setFakeSearchResult(
        [...fakeSearchResults, TakeScreenshotSearchResult]);
    assertTrue(dropdownElement.opened);
    assertEquals(3, searchBoxElement.searchResults.length);

    // Trigger the observer callback.
    searchBoxElement.onSearchResultsAvailabilityChanged();
    await waitForSearchResultsFetched();
    // Check that the list updates when the dropdown is open, and the dropdown
    // remains open.
    assertTrue(dropdownElement.opened);
    assertEquals(4, searchBoxElement.searchResults.length);

    // User clicks outside the search box, closing the dropdown.
    searchBoxElement.blur();
    assertFalse(dropdownElement.opened);

    // Reset the fake results.
    handler.setFakeSearchResult(fakeSearchResults);

    // Dropdown should still be closed, and the search results should not have
    // updated yet.
    assertFalse(dropdownElement.opened);
    assertEquals(4, searchBoxElement.searchResults.length);

    // Trigger the observer callback.
    searchBoxElement.onSearchResultsAvailabilityChanged();
    await waitForSearchResultsFetched();

    // Check that the list updates when the dropdown is closed, and the dropdown
    // remains closed.
    assertFalse(dropdownElement.opened);
    assertEquals(3, searchBoxElement.searchResults.length);

    // The first item should be selected immediately when the search results
    // change even if the change occurred while the dropdown was closed.
    searchFieldElement.getSearchInput().focus();
    await waitForListUpdate();
    assertTrue(dropdownElement.opened);
    const selectedRow = strictQuery(
        'search-result-row[selected]', dropdownElement, SearchResultRowElement);
    const selectedItem =
        resultsListElement.selectedItem as (MojoSearchResult | undefined);
    assertTrue(!!selectedItem);
    assertEquals(
        selectedRow.searchResult.acceleratorLayoutInfo.description,
        selectedItem.acceleratorLayoutInfo.description);
  });

  test(
      'Filter partially disabled search results', async () => {
        [searchBoxElement, searchFieldElement, dropdownElement,
         resultsListElement] = initSearchBoxElement();

        const disabledFirstAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[0]!;
        disabledFirstAcceleratorInfo.state =
            AcceleratorState.kDisabledByUnavailableKeys;

        // This SearchResult is of standard layout, and contains two
        // AcceleratorInfos. We disable one of them to verify that the disabled
        // AcceleratorInfo is not shown.
        const partiallyDisabledSearchResult: MojoSearchResult = {
          ...TakeScreenshotSearchResult,
          acceleratorInfos: [
            disabledFirstAcceleratorInfo,
            TakeScreenshotSearchResult.acceleratorInfos[1]!,
          ],
        };

        // Set search results: one is partially disabled, the other is enabled.
        handler.setFakeSearchResult(
            [partiallyDisabledSearchResult, CycleTabsTextSearchResult]);

        await simulateSearch('query');

        assertTrue(dropdownElement.opened);
        assertEquals(2, searchBoxElement.searchResults.length);

        // Check that the disabled AcceleratorInfo is not present in the
        // acceleratorInfos list of the partially disabled search result.
        assertEquals(
            1, searchBoxElement.searchResults[0]?.acceleratorInfos.length);
        assertEquals(
            TakeScreenshotSearchResult.acceleratorInfos[1],
            searchBoxElement.searchResults[0]?.acceleratorInfos[0]);
      });

  test(
      'Filter fully disabled search results when customization is allowed',
      async () => {
        loadTimeData.overrideValues({isCustomizationAllowed: true});

        [searchBoxElement, searchFieldElement, dropdownElement,
         resultsListElement] = initSearchBoxElement();

        const disabledFirstAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[0]!;
        const disabledSecondAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[1]!;
        disabledFirstAcceleratorInfo.state =
            AcceleratorState.kDisabledByUnavailableKeys;
        disabledSecondAcceleratorInfo.state = AcceleratorState.kDisabledByUser;

        // Create a SearchResult that doesn't have any enabled AcceleratorInfos,
        // one is disabled by unavailable keys, the other is disabled by user.
        const fullyDisabledSearchResult: MojoSearchResult = {
          ...TakeScreenshotSearchResult,
          acceleratorInfos:
              [disabledFirstAcceleratorInfo, disabledSecondAcceleratorInfo],
        };

        handler.setFakeSearchResult([fullyDisabledSearchResult]);

        searchBoxElement.onSearchResultsAvailabilityChanged();
        await simulateSearch('query');

        assertTrue(dropdownElement.opened);
        // Verify fully disabled search result is not filtered out.
        assertEquals(1, searchBoxElement.searchResults.length);

        const searchResultRowElement = strictQuery(
            'search-result-row', searchBoxElement.shadowRoot,
            SearchResultRowElement);
        assertTrue(!!searchResultRowElement);

        // Verify description is shown.
        assertEquals(
            'Take full screenshot or screen recording',
            strictQuery(
                '#description', searchResultRowElement.shadowRoot,
                HTMLDivElement)
                .innerText);

        // Verify 'No shortcut assigned' message is shown.
        assertEquals(
            'No shortcut assigned',
            strictQuery(
                '#noShortcutAssignedMessage', searchResultRowElement.shadowRoot,
                HTMLDivElement)
                .innerText);
      });

  test(
      'Filter fully disabled search results when customization is not allowed',
      async () => {
        loadTimeData.overrideValues({isCustomizationAllowed: false});
        [searchBoxElement, searchFieldElement, dropdownElement,
         resultsListElement] = initSearchBoxElement();

        const disabledFirstAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[0]!;
        const disabledSecondAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[1]!;
        disabledFirstAcceleratorInfo.state =
            AcceleratorState.kDisabledByUnavailableKeys;
        disabledSecondAcceleratorInfo.state = AcceleratorState.kDisabledByUser;

        // Create a SearchResult that doesn't have any enabled AcceleratorInfos,
        // one is disabled by unavailable keys, the other is disabled by user.
        const fullyDisabledSearchResult: MojoSearchResult = {
          ...TakeScreenshotSearchResult,
          acceleratorInfos:
              [disabledFirstAcceleratorInfo, disabledSecondAcceleratorInfo],
        };

        handler.setFakeSearchResult([fullyDisabledSearchResult]);

        searchBoxElement.onSearchResultsAvailabilityChanged();
        await simulateSearch('query');

        assertTrue(dropdownElement.opened);
        // Verify fully disabled search result is hidden when customization is
        // not allowed.
        assertEquals(0, searchBoxElement.searchResults.length);
      });

  test(
      'Filter disabled + ensure extra results are present when customization' +
          'is not allowed',
      async () => {
        loadTimeData.overrideValues({isCustomizationAllowed: false});
        [searchBoxElement, searchFieldElement, dropdownElement,
         resultsListElement] = initSearchBoxElement();
        // Create a SearchResult that doesn't have any enabled AcceleratorInfos.
        const disabledFirstAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[0]!;
        disabledFirstAcceleratorInfo.state =
            AcceleratorState.kDisabledByUnavailableKeys;
        const disabledSecondAcceleratorInfo =
            TakeScreenshotSearchResult.acceleratorInfos[1]!;
        disabledSecondAcceleratorInfo.state = AcceleratorState.kDisabledByUser;
        const fullyDisabledSearchResult: MojoSearchResult = {
          ...TakeScreenshotSearchResult,
          acceleratorInfos:
              [disabledFirstAcceleratorInfo, disabledSecondAcceleratorInfo],
        };

        handler.setFakeSearchResult([
          fullyDisabledSearchResult,
          CycleTabsTextSearchResult,
          CycleTabsTextSearchResult,
          CycleTabsTextSearchResult,
          CycleTabsTextSearchResult,
          CycleTabsTextSearchResult,
          CycleTabsTextSearchResult,
        ]);

        searchBoxElement.onSearchResultsAvailabilityChanged();
        await simulateSearch('query');

        assertTrue(dropdownElement.opened);
        // After filtering, at most 5 of the non-disabled elements should be
        // shown.
        assertEquals(5, searchBoxElement.searchResults.length);
        assertEquals(
            CycleTabsTextSearchResult.acceleratorLayoutInfo.description,
            searchBoxElement.searchResults[0]
                ?.acceleratorLayoutInfo.description);
      });

  test('Max query length has been set', async () => {
    [searchBoxElement, searchFieldElement, dropdownElement,
     resultsListElement] = initSearchBoxElement();

    assertTrue(!!searchFieldElement.getSearchInput().maxLength);
  });
});
