// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_result_row.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {ShortcutInputKeyElement} from 'chrome://resources/ash/common/shortcut_input_ui/shortcut_input_key.js';
import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {mojoString16ToString} from 'chrome://resources/js/mojo_type_util.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AcceleratorLookupManager} from 'chrome://shortcut-customization/js/accelerator_lookup_manager.js';
import {CycleTabsTextSearchResult, fakeAcceleratorConfig, fakeLayoutInfo, SnapWindowLeftSearchResult, TakeScreenshotSearchResult} from 'chrome://shortcut-customization/js/fake_data.js';
import {getBoldedDescription} from 'chrome://shortcut-customization/js/search/search_result_bolding.js';
import {SearchResultRowElement} from 'chrome://shortcut-customization/js/search/search_result_row.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

function initSearchResultRowElement(): SearchResultRowElement {
  const element = document.createElement('search-result-row');
  // Add required property `searchQuery` since it is used to determine which
  // parts of the description to bold in the SearchResultRowElement.
  element.searchQuery = '';
  document.body.appendChild(element);
  flush();
  return element;
}

suite('searchResultRowTest', function() {
  let searchResultRowElement: SearchResultRowElement|null = null;
  let manager: AcceleratorLookupManager|null = null;

  setup(() => {
    // Set up manager.
    manager = AcceleratorLookupManager.getInstance();
    manager.setAcceleratorLookup(fakeAcceleratorConfig);
    manager.setAcceleratorLayoutLookup(fakeLayoutInfo);
  });

  teardown(() => {
    if (manager) {
      manager.reset();
    }
    if (searchResultRowElement) {
      searchResultRowElement.remove();
    }
    searchResultRowElement = null;
  });

  test('SearchResultRowLoaded', async () => {
    searchResultRowElement = initSearchResultRowElement();
    await flush();
    assertTrue(!!searchResultRowElement);
  });

  test('SearchResultDescriptionDisplayed', async () => {
    searchResultRowElement = initSearchResultRowElement();
    await flush();
    const searchResultTextElement =
        searchResultRowElement!.shadowRoot!.querySelector('#description');
    assertTrue(!!searchResultTextElement);

    searchResultRowElement.searchResult = SnapWindowLeftSearchResult;
    await flush();
    assertEquals(
        'Snap Window Left', searchResultTextElement.textContent?.trim());
  });

  test('Text accelerators render correctly', async () => {
    searchResultRowElement = initSearchResultRowElement();
    searchResultRowElement.searchResult = CycleTabsTextSearchResult;
    await flush();
    assertEquals(
        'Click or tap shelf icons 1-8',
        strictQuery(
            '#description', searchResultRowElement.shadowRoot, HTMLDivElement)
            .innerText);

    // Create a new TextAccelerator to compare against the one rendered by the
    // SearchResultRow.
    const textAccelElement = document.createElement('text-accelerator');
    textAccelElement.parts = CycleTabsTextSearchResult.acceleratorInfos[0]
                                 ?.layoutProperties.textAccelerator?.parts!;
    document.body.appendChild(textAccelElement);
    await flushTasks();
    const searchResultRowTextAccelerator = strictQuery(
        'text-accelerator', searchResultRowElement.shadowRoot,
        TextAcceleratorElement);
    assertEquals(
        textAccelElement.innerHTML, searchResultRowTextAccelerator.innerHTML);

    // Select the row and verify that the text accelerator is highlighted.
    assertFalse(searchResultRowTextAccelerator.highlighted);
    searchResultRowElement.selected = true;
    assertTrue(searchResultRowTextAccelerator.highlighted);
  });

  test('Standard accelerators render correctly', () => {
    searchResultRowElement = initSearchResultRowElement();
    searchResultRowElement.searchResult = TakeScreenshotSearchResult;
    flush();

    const searchResultDescription =
        searchResultRowElement.shadowRoot!.querySelector('#description');
    assertTrue(!!searchResultDescription);
    assertEquals(
        'Take full screenshot or screen recording',
        searchResultDescription.textContent?.trim());

    const acceleratorElements =
        searchResultRowElement.shadowRoot!.querySelectorAll<HTMLDivElement>(
            '.accelerator-keys');
    // Two accelerators are expected.
    assertEquals(2, acceleratorElements.length);

    const keys1: NodeListOf<ShortcutInputKeyElement> =
        acceleratorElements[0]!.querySelectorAll('shortcut-input-key');
    // Control + Overview
    assertEquals(2, keys1.length);
    assertEquals(
        'ctrl',
        keys1[0]!.shadowRoot!.querySelector('#key')!.textContent!.trim());
    assertEquals(
        'show windows',
        keys1[1]!.shadowRoot!.querySelector('#keyIcon')!.getAttribute(
            'aria-label'));

    const keys2: NodeListOf<ShortcutInputKeyElement> =
        acceleratorElements[1]!.querySelectorAll('shortcut-input-key');
    // Screenshot
    assertEquals(1, keys2.length);
    assertEquals(
        'take screenshot',
        keys2[0]!.shadowRoot!.querySelector('#keyIcon')!.getAttribute(
            'aria-label'));

    // Select the row and verify that the keys are highlighted.
    assertFalse(keys1[0]!.highlighted);
    assertFalse(keys1[1]!.highlighted);
    assertFalse(keys2[0]!.highlighted);
    searchResultRowElement.selected = true;
    assertTrue(keys1[0]!.highlighted);
    assertTrue(keys1[1]!.highlighted);
    assertTrue(keys2[0]!.highlighted);
  });

  test('Standard accelerators have correct text dividers', async () => {
    searchResultRowElement = initSearchResultRowElement();
    searchResultRowElement.searchResult = TakeScreenshotSearchResult;
    flush();

    // "or" divider should only be present once, between search results.
    assertEquals(
        1,
        searchResultRowElement.shadowRoot
            ?.querySelectorAll('.accelerator-text-divider')
            .length,
        'Divider should be present once, between search results.');
    assertEquals(
        'or',
        searchResultRowElement.shadowRoot
            ?.querySelector('.accelerator-text-divider')
            ?.textContent?.trim());

    // For search results with only one set of accelerators, there should not be
    // a text divider present.
    searchResultRowElement = initSearchResultRowElement();
    searchResultRowElement.searchResult = SnapWindowLeftSearchResult;
    flush();
    assertEquals(
        0,
        searchResultRowElement.shadowRoot
            ?.querySelectorAll('.accelerator-text-divider')
            .length);
  });

  test('Query-matching bolded results show up correctly', async () => {
    const query = 'Take screenshot';
    searchResultRowElement = initSearchResultRowElement();
    searchResultRowElement.searchQuery = query;
    searchResultRowElement.searchResult = TakeScreenshotSearchResult;
    flush();
    assertEquals(
        getBoldedDescription(
            mojoString16ToString(searchResultRowElement.searchResult
                                     .acceleratorLayoutInfo.description),
            query)
            .toString(),
        strictQuery(
            '#description', searchResultRowElement.shadowRoot, HTMLDivElement)
            .innerHTML);
  });

  test('Aria labels are correct', async () => {
    searchResultRowElement = initSearchResultRowElement();
    await flush();

    // Standard accelerator info
    searchResultRowElement.focusRowIndex = 2;
    searchResultRowElement.listLength = 4;
    searchResultRowElement.searchResult = SnapWindowLeftSearchResult;
    await flush();
    assertEquals(
        'Search result 3 of 4: Snap Window Left, alt [. ' +
            'Press Enter to navigate to shortcut.',
        searchResultRowElement.ariaLabel);

    // Text accelerator info
    searchResultRowElement.focusRowIndex = 1;
    searchResultRowElement.listLength = 5;
    searchResultRowElement.searchResult = CycleTabsTextSearchResult;
    await flush();
    assertEquals(
        'Search result 2 of 5: Click or tap shelf icons 1-8, ctrl + 1 through' +
            ' 8. Press Enter to navigate to shortcut.',
        searchResultRowElement.ariaLabel);
  });
});
