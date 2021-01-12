// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {InfiniteList, TabSearchItem} from 'chrome://tab-search/tab_search.js';

import {assertEquals, assertGT, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {generateSampleTabsFromSiteNames, sampleSiteNames} from './tab_search_test_data.js';
import {assertTabItemAndNeighborsInViewBounds, disableScrollIntoViewAnimations} from './tab_search_test_helper.js';

const CHUNK_ITEM_COUNT = 7;

class TestApp extends PolymerElement {
  static get properties() {
    return {
      /** @private {number} */
      chunkItemCount_: {
        type: Number,
        value: CHUNK_ITEM_COUNT
      }
    };
  }

  static get template() {
    return html`
    <infinite-list id="list" chunk-item-count="[[chunkItemCount_]]">
      <template is="dom-repeat">
        <tab-search-item id="[[item.tab.tabId]]"
            style="display: flex;height: 56px" data="[[item]]" tabindex="0"
            role="option">
        </tab-search-item>
      </template>
    </infinite-list>`;
  }
}

customElements.define('test-app', TestApp);

suite('InfiniteListTest', () => {
  /** @type {!InfiniteList} */
  let infiniteList;

  disableScrollIntoViewAnimations(TabSearchItem);

  /**
   * @param {!Array<!tabSearch.mojom.Tab>} sampleData
   */
  async function setupTest(sampleData) {
    document.head.insertAdjacentHTML('beforeend', `
    <style>
    html {
      --list-max-height: 280px;
    }
    </style>`);

    const testApp = document.createElement('test-app');
    document.body.innerHTML = '';
    document.body.appendChild(testApp);

    infiniteList = /** @type {!InfiniteList} */ (
        testApp.shadowRoot.querySelector('#list'));
    infiniteList.items = sampleData;
    await flushTasks();
  }

  /**
   * @return {!NodeList<!HTMLElement>}
   */
  function queryRows() {
    return /** @type {!NodeList<!HTMLElement>} */ (
        infiniteList.querySelectorAll('tab-search-item'));
  }

  /**
   * @param {!Array<string>} siteNames
   * @return {!Array}
   */
  function sampleTabItems(siteNames) {
    return generateSampleTabsFromSiteNames(siteNames).map(tab => {
      return {hostname: new URL(tab.url).hostname, tab};
    });
  }

  test('ScrollHeight', async () => {
    const tabItems = sampleTabItems(sampleSiteNames());
    await setupTest(tabItems);

    assertEquals(0, infiniteList.scrollTop);

    const itemHeightStyle =
        getComputedStyle(document.head).getPropertyValue('--mwb-item-height');
    assertTrue(itemHeightStyle.endsWith('px'));

    const tabItemHeight = Number.parseInt(
        itemHeightStyle.substring(0, itemHeightStyle.length - 2), 10);
    assertEquals(tabItemHeight * tabItems.length, infiniteList.scrollHeight);
  });

  test('SelectedIndex', async () => {
    const siteNames = Array.from({length: 50}, (_, i) => 'site' + (i + 1));
    const tabItems = sampleTabItems(siteNames);
    await setupTest(tabItems);

    assertEquals(0, infiniteList.scrollTop);
    assertEquals(CHUNK_ITEM_COUNT, queryRows().length);

    // Assert that upon changing the selected index to a non previously rendered
    // item, this one is rendered on the view.
    infiniteList.selected = CHUNK_ITEM_COUNT;
    assertEquals(CHUNK_ITEM_COUNT, queryRows().length);
    await waitAfterNextRender(infiniteList);
    let domTabItems = queryRows();
    const selectedTabItem = domTabItems[infiniteList.selected];
    assertNotEquals(null, selectedTabItem);
    assertEquals(2 * CHUNK_ITEM_COUNT, domTabItems.length);

    // Assert that the view scrolled to show the selected item.
    const afterSelectionScrollTop = infiniteList.scrollTop;
    assertNotEquals(0, afterSelectionScrollTop);

    // Assert that on replacing the list items, the currently selected index
    // value is still rendered on the view.
    infiniteList.items = sampleTabItems(siteNames);
    await waitAfterNextRender(infiniteList);
    domTabItems = queryRows();
    const theSelectedTabItem = domTabItems[infiniteList.selected];
    assertNotEquals(null, theSelectedTabItem);

    // Assert the selected item is still visible in the view.
    assertEquals(afterSelectionScrollTop, infiniteList.scrollTop);
  });

  test('NavigateDownShowsPreviousAndFollowingListItems', async () => {
    const tabItems = sampleTabItems(sampleSiteNames());
    await setupTest(tabItems);

    const tabsDiv = /** @type {!HTMLElement} */ (infiniteList);
    // Assert that the tabs are in a overflowing state.
    assertGT(tabsDiv.scrollHeight, tabsDiv.clientHeight);

    infiniteList.selected = 0;
    for (let i = 0; i < tabItems.length; i++) {
      infiniteList.navigate('ArrowDown');
      await waitAfterNextRender(infiniteList);

      const selectedIndex = ((i + 1) % tabItems.length);
      assertEquals(selectedIndex, infiniteList.selected);
      assertTabItemAndNeighborsInViewBounds(
          tabsDiv, queryRows(), selectedIndex);
    }
  });

  test('NavigateUpShowsPreviousAndFollowingListItems', async () => {
    const tabItems = sampleTabItems(sampleSiteNames());
    await setupTest(tabItems);

    const tabsDiv = /** @type {!HTMLElement} */ (infiniteList);
    // Assert that the tabs are in a overflowing state.
    assertGT(tabsDiv.scrollHeight, tabsDiv.clientHeight);

    infiniteList.selected = 0;
    for (let i = tabItems.length; i > 0; i--) {
      infiniteList.navigate('ArrowUp');
      await waitAfterNextRender(infiniteList);

      const selectIndex = (i - 1 + tabItems.length) % tabItems.length;
      assertEquals(selectIndex, infiniteList.selected);
      assertTabItemAndNeighborsInViewBounds(tabsDiv, queryRows(), selectIndex);
    }
  });
});
