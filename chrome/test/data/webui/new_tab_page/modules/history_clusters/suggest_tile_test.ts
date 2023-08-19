// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SuggestTileModuleElement} from 'chrome://new-tab-page/lazy_load.js';
import {$$} from 'chrome://new-tab-page/new_tab_page.js';
import {assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {createRelatedSearches, MIN_RELATED_SEARCHES} from './test_support.js';

suite('NewTabPageModulesHistoryClustersModuleSuggestTileTest', () => {
  let suggestTileElement: SuggestTileModuleElement;

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    suggestTileElement = new SuggestTileModuleElement();
    suggestTileElement.relatedSearches =
        createRelatedSearches(MIN_RELATED_SEARCHES);
    document.body.append(suggestTileElement);
    await waitAfterNextRender(suggestTileElement);
  });

  test('Related searches element populated with correct data', async () => {
    assertTrue(!!suggestTileElement);

    assertEquals($$(suggestTileElement, '.title')!.innerHTML, 'Test Query 0');

    assertEquals(
        suggestTileElement.shadowRoot!.querySelectorAll('.title').length, 3);
  });
});
