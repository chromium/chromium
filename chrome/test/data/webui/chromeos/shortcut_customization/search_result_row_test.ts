// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_result_row.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {strictQuery} from 'chrome://resources/ash/common/typescript_utils/strict_query.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {CycleTabsTextSearchResult, SnapWindowLeftSearchResult} from 'chrome://shortcut-customization/js/fake_data.js';
import {SearchResultRowElement} from 'chrome://shortcut-customization/js/search/search_result_row.js';
import {TextAcceleratorElement} from 'chrome://shortcut-customization/js/text_accelerator.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';

function initSearchResultRowElement(): SearchResultRowElement {
  const element = document.createElement('search-result-row');
  document.body.appendChild(element);
  flush();
  return element;
}

suite('searchResultRowTest', function() {
  let searchResultRowElement: SearchResultRowElement|null = null;

  teardown(() => {
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
  });
});
