// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import 'chrome://shortcut-customization/js/search/search_result_row.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {SnapWindowLeftSearchResult} from 'chrome://shortcut-customization/js/fake_data.js';
import {SearchResultRowElement} from 'chrome://shortcut-customization/js/search/search_result_row.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

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

  test('SearchResultTextDisplayed', async () => {
    searchResultRowElement = initSearchResultRowElement();
    await flush();
    const searchResultTextElement =
        searchResultRowElement!.shadowRoot!.querySelector('#searchResultText');
    assertTrue(!!searchResultTextElement);

    searchResultRowElement.searchResult = SnapWindowLeftSearchResult;
    await flush();
    assertEquals('Snap Window Left', searchResultTextElement.textContent);
  });
});
