// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {InfiniteList, TabData, TabSearchItem, TitleItem} from 'chrome://tab-search.top-chrome/tab_search.js';

import {assertEquals, assertGT, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {generateSampleTabsFromSiteNames, sampleSiteNames} from './tab_search_test_data.js';
import {assertTabItemAndNeighborsInViewBounds, disableAnimationBehavior} from './tab_search_test_helper.js';

const SAMPLE_AVAIL_HEIGHT = 336;
const SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT = 6;
const SAMPLE_SECTION_CLASS = 'section-title';

class TestApp extends PolymerElement {
  static get properties() {
    return {
      /** @private {number} */
      maxHeight_: {
        type: Number,
        value: SAMPLE_AVAIL_HEIGHT,
      },
    };
  }

  static get template() {
    return html`
    <infinite-list id="list" max-height="[[maxHeight_]]">
      <template data-type="TitleItem">
        <div class="section-title">[[item.title]]</div>
      </template>
      <template data-type="TabData" data-selectable>
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

  disableAnimationBehavior(InfiniteList, 'scrollTo');
  disableAnimationBehavior(TabSearchItem, 'scrollIntoView');

  /**
   * @param {!Array<!TabData>} sampleData
   */
  async function setupTest(sampleData) {
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
   * @param {string} className
   * @return {!NodeList<!HTMLElement>}
   */
  function queryClassElements(className) {
    return /** @type {!NodeList<!HTMLElement>} */ (
        infiniteList.querySelectorAll('.' + className));
  }

  /**
   * @param {!Array<string>} siteNames
   * @return {!Array<!TabData>}
   */
  function sampleTabItems(siteNames) {
    return generateSampleTabsFromSiteNames(siteNames).map(tab => {
      const tabData = {hostname: new URL(tab.url).hostname, tab};
      Object.setPrototypeOf(tabData, TabData.prototype);
      return tabData;
    });
  }

  test('ScrollHeight', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(5));
    await setupTest(tabItems);
    await waitAfterNextRender(infiniteList);

    assertEquals(0, infiniteList.scrollTop);

    const itemHeightStyle =
        getComputedStyle(document.head).getPropertyValue('--mwb-item-height');
    assertTrue(itemHeightStyle.endsWith('px'));

    const tabItemHeight = Number.parseInt(
        itemHeightStyle.substring(0, itemHeightStyle.length - 2), 10);
    assertEquals(tabItemHeight * tabItems.length, infiniteList.scrollHeight);
  });

  test('ListUpdates', async () => {
    await setupTest(sampleTabItems(sampleSiteNames(1)));
    assertEquals(1, queryRows().length);

    // Ensure that on updating the list with an array smaller in size
    // than the viewport item count, all the array items are rendered.
    infiniteList.items = sampleTabItems(sampleSiteNames(3));
    await waitAfterNextRender(infiniteList);
    assertEquals(3, queryRows().length);

    // Ensure that on updating the list with an array greater in size than
    // the viewport item count, only a chunk of array items are rendered.
    const tabItems =
        sampleTabItems(sampleSiteNames(2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT));
    infiniteList.items = tabItems;
    await waitAfterNextRender(infiniteList);
    assertGT(tabItems.length, queryRows().length);
  });

  test('SelectedIndex', async () => {
    const itemCount = 25;
    const tabItems = sampleTabItems(sampleSiteNames(itemCount));
    await setupTest(tabItems);

    assertEquals(0, infiniteList.scrollTop);

    // Assert that upon changing the selected index to a non previously rendered
    // item, this one is rendered on the view.
    infiniteList.selected = itemCount - 1;
    await waitAfterNextRender(infiniteList);
    let domTabItems = queryRows();
    const selectedTabItem = domTabItems[infiniteList.selected];
    assertNotEquals(null, selectedTabItem);
    assertEquals(25, domTabItems.length);

    // Assert that the view scrolled to show the selected item.
    const afterSelectionScrollTop = infiniteList.scrollTop;
    assertNotEquals(0, afterSelectionScrollTop);

    // Assert that on replacing the list items, the currently selected index
    // value is still rendered on the view.
    infiniteList.items = sampleTabItems(sampleSiteNames(itemCount));
    await waitAfterNextRender(infiniteList);
    domTabItems = queryRows();
    const theSelectedTabItem = domTabItems[infiniteList.selected];
    assertNotEquals(null, theSelectedTabItem);

    // Assert the selected item is still visible in the view.
    assertEquals(afterSelectionScrollTop, infiniteList.scrollTop);
  });

  test('SelectedIndexValidAfterItemRemoval', async () => {
    const numTabItems = 5;
    const tabItems = sampleTabItems(sampleSiteNames(numTabItems));
    await setupTest(tabItems);
    infiniteList.selected = numTabItems - 1;

    // Assert that on having the last item selected and removing this last item
    // the selected index moves up to the last item available and that in
    // the case there are no more items, the selected index is -1.
    for (let i = numTabItems - 1; i >= 0; i--) {
      infiniteList.items = tabItems.slice(0, i);
      await waitAfterNextRender(infiniteList);
      assertEquals(i, queryRows().length);
      assertEquals(i - 1, infiniteList.selected);
    }
  });

  test('NavigateDownShowsPreviousAndFollowingListItems', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(10));
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
    const tabItems = sampleTabItems(sampleSiteNames(10));
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

  test('ListSelection', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(2));
    const listItems = [
      new TitleItem('Title 1'), ...tabItems, new TitleItem('Title 2'),
      ...tabItems
    ];

    setupTest(listItems);
    assertEquals(2, queryClassElements(SAMPLE_SECTION_CLASS).length);
    assertEquals(4, queryRows().length);

    infiniteList.selected = 0;
    await waitAfterNextRender(infiniteList);

    const selectableItemCount = 2 * tabItems.length;
    for (let i = 0; i < selectableItemCount; i++) {
      infiniteList.navigate('ArrowDown');
      await waitAfterNextRender(infiniteList);

      assertEquals((i + 1) % selectableItemCount, infiniteList.selected);
      assertTrue(infiniteList.selectedItem instanceof TabData);
    }

    infiniteList.items = [];
    await waitAfterNextRender(infiniteList);
    assertEquals(0, queryClassElements(SAMPLE_SECTION_CLASS).length);
    assertEquals(0, queryRows().length);
  });
});
