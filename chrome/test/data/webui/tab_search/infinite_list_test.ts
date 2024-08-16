// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {InfiniteList, TabData, TabItemType, TabSearchItemElement, TitleItem} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {generateSampleTabsFromSiteNames, sampleSiteNames} from './tab_search_test_data.js';
import {assertTabItemAndNeighborsInViewBounds, disableAnimationBehavior} from './tab_search_test_helper.js';

const SAMPLE_AVAIL_HEIGHT = 336;
const SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT = 6;
const SAMPLE_SECTION_CLASS = 'section-title';

class TestApp extends CrLitElement {
  static get is() {
    return 'test-app';
  }

  static override get properties() {
    return {
      maxHeight_: {type: Number},
    };
  }

  override render() {
    return html`
    <infinite-list max-height="${this.maxHeight_}"
        .isSelectable=${(item: any) => item.constructor.name === 'TabData'}
        .template=${(item: any) => {
      switch (item.constructor.name) {
        case 'TitleItem':
          return html`<div class="section-title">${item.title}</div>`;
        case 'TabData':
          return html`<tab-search-item id="${item.tab.tabId}"
                class="selectable"
                style="display: flex;height: 48px" .data="${item}" tabindex="0"
                role="option">
            </tab-search-item>`;
        default:
          return '';
      }
    }}
    </infinite-list>`;
  }

  private maxHeight_: number = SAMPLE_AVAIL_HEIGHT;
}

customElements.define('test-app', TestApp);

suite('InfiniteListTest', () => {
  let infiniteList: InfiniteList;

  disableAnimationBehavior(InfiniteList, 'scrollTo');
  disableAnimationBehavior(TabSearchItemElement, 'scrollIntoView');

  async function setupTest(sampleData: Array<TabData|TitleItem>) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const testApp = document.createElement('test-app');
    document.body.appendChild(testApp);

    infiniteList = testApp.shadowRoot!.querySelector('infinite-list')!;
    infiniteList.items = sampleData;
    await microtasksFinished();
  }

  function queryRows(): NodeListOf<HTMLElement> {
    return infiniteList.querySelectorAll('tab-search-item');
  }

  function queryClassElements(className: string): NodeListOf<HTMLElement> {
    return infiniteList.querySelectorAll('.' + className);
  }

  function sampleTabItems(siteNames: string[]): TabData[] {
    return generateSampleTabsFromSiteNames(siteNames).map(tab => {
      return new TabData(
          tab, TabItemType.OPEN_TAB, new URL(tab.url.url).hostname);
    });
  }

  test('ScrollHeight', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(5));
    await setupTest(tabItems);

    assertEquals(0, infiniteList.scrollTop);

    const paddingBottomStyle =
        getComputedStyle(infiniteList).getPropertyValue('padding-bottom');
    assertTrue(paddingBottomStyle.endsWith('px'));

    const paddingBottom = Number.parseInt(
        paddingBottomStyle.substring(0, paddingBottomStyle.length - 2), 10);

    const itemHeightStyle =
        getComputedStyle(document.head).getPropertyValue('--mwb-item-height');
    assertTrue(itemHeightStyle.endsWith('px'));

    const tabItemHeight = Number.parseInt(
        itemHeightStyle.substring(0, itemHeightStyle.length - 2), 10);
    assertEquals(
        tabItemHeight * tabItems.length + paddingBottom,
        infiniteList.scrollHeight);
  });

  test('ListUpdates', async () => {
    await setupTest(sampleTabItems(sampleSiteNames(1)));
    assertEquals(1, queryRows().length);

    // Ensure that on updating the list with an array smaller in size
    // than the viewport item count, all the array items are rendered.
    infiniteList.items = sampleTabItems(sampleSiteNames(3));
    await microtasksFinished();
    assertEquals(3, queryRows().length);

    // Ensure that on updating the list with an array greater in size than
    // the viewport item count, only a chunk of array items are rendered.
    const tabItems =
        sampleTabItems(sampleSiteNames(2 * SAMPLE_HEIGHT_VIEWPORT_ITEM_COUNT));
    infiniteList.items = tabItems;
    await microtasksFinished();
    assertGT(tabItems.length, queryRows().length);
  });

  test('SelectedIndex', async () => {
    const itemCount = 25;
    const tabItems = sampleTabItems(sampleSiteNames(itemCount));
    await setupTest(tabItems);

    assertEquals(0, infiniteList.scrollTop);

    // Assert that upon changing the selected index to a non previously rendered
    // item, this one is rendered on the view.
    await infiniteList.setSelected(itemCount - 1);
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
    await microtasksFinished();
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
    await infiniteList.setSelected(numTabItems - 1);

    // Assert that on having the last item selected and removing this last item
    // the selected index moves up to the last item available and that in
    // the case there are no more items, the selected index is -1.
    for (let i = numTabItems - 1; i >= 0; i--) {
      infiniteList.items = tabItems.slice(0, i);
      await microtasksFinished();
      assertEquals(i, queryRows().length);
      assertEquals(i - 1, infiniteList.selected);
    }
  });

  test('NavigateDownShowsPreviousAndFollowingListItems', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(10));
    await setupTest(tabItems);

    // Assert that the tabs are in a overflowing state.
    assertGT(infiniteList.scrollHeight, infiniteList.clientHeight);

    await infiniteList.setSelected(0);
    for (let i = 0; i < tabItems.length; i++) {
      await infiniteList.navigate('ArrowDown');
      await microtasksFinished();

      const selectedIndex = ((i + 1) % tabItems.length);
      assertEquals(selectedIndex, infiniteList.selected);
      assertTabItemAndNeighborsInViewBounds(
          infiniteList, queryRows(), selectedIndex);
    }
  });

  test('NavigateUpShowsPreviousAndFollowingListItems', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(10));
    await setupTest(tabItems);

    // Assert that the tabs are in a overflowing state.
    assertGT(infiniteList.scrollHeight, infiniteList.clientHeight);

    await infiniteList.setSelected(0);
    for (let i = tabItems.length; i > 0; i--) {
      await infiniteList.navigate('ArrowUp');

      const selectIndex = (i - 1 + tabItems.length) % tabItems.length;
      assertEquals(selectIndex, infiniteList.selected);
      assertTabItemAndNeighborsInViewBounds(
          infiniteList, queryRows(), selectIndex);
    }
  });

  test('ListSelection', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(2));
    const listItems = [
      new TitleItem('Title 1'),
      ...tabItems,
      new TitleItem('Title 2'),
      ...tabItems,
    ];

    await setupTest(listItems);
    assertEquals(2, queryClassElements(SAMPLE_SECTION_CLASS).length);
    assertEquals(4, queryRows().length);

    await infiniteList.setSelected(1);

    const itemCount = 2 * tabItems.length + 2;
    for (let i = 1; i < itemCount; i++) {
      if (listItems[i] instanceof TitleItem) {
        // Title items should be skipped.
        assertEquals(i + 1, infiniteList.selected);
        continue;
      }
      await infiniteList.navigate('ArrowDown');
      // Navigation increments by 1 in most cases, or by 2 to skip over a title
      // item.
      const expectedIndex =
          listItems[(i + 1) % itemCount] instanceof TitleItem ? i + 2 : i + 1;
      assertEquals(expectedIndex % itemCount, infiniteList.selected);
      assertTrue(!!infiniteList.selectedItem);
      const item = infiniteList.selectedItem as TabSearchItemElement;
      assertTrue(item.data instanceof TabData);
    }

    infiniteList.items = [];
    await microtasksFinished();
    assertEquals(0, queryClassElements(SAMPLE_SECTION_CLASS).length);
    assertEquals(0, queryRows().length);
  });

  test('FillCurrentViewHeightRendersOnlySomeItems', async () => {
    const tabItems = sampleTabItems(sampleSiteNames(10));
    await setupTest(tabItems);

    // Assert that the tabs are in an overflowing state.
    assertGT(infiniteList.scrollHeight, infiniteList.clientHeight);

    // Assert that not all tab items are shown.
    const initialRows = queryRows().length;
    assertGT(tabItems.length, initialRows);

    // fillCurrentViewHeight() should only render enough items to fill the
    // view. Since the view height has not changed, no new items should
    // render.
    await infiniteList.fillCurrentViewHeight();
    await microtasksFinished();
    assertEquals(initialRows, queryRows().length);

    // ensureAllDomItemsAvailable() should render everything.
    await infiniteList.ensureAllDomItemsAvailable();
    await microtasksFinished();
    assertEquals(tabItems.length, queryRows().length);
  });

  test('HiddenWhenUpdated', async () => {
    const testApp = document.createElement('test-app');
    testApp.style.display = 'none';
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(testApp);

    infiniteList = testApp.shadowRoot!.querySelector('infinite-list')!;
    infiniteList.items = sampleTabItems(sampleSiteNames(10));
    await microtasksFinished();

    // infinite-list will try to render one item, and then return early
    // since the DOM item height cannot be estimated. List item and container
    // are not visible.
    assertEquals(1, queryRows().length);
    let firstItem = queryRows()[0]!;
    assertFalse(isVisible(infiniteList.$.container));
    assertFalse(isVisible(firstItem));

    testApp.style.display = '';
    await microtasksFinished();

    // After the client is rendered and calls fillCurrentViewHeight(), the
    // container and item should be visible.
    await infiniteList.fillCurrentViewHeight();
    await microtasksFinished();
    assertGT(queryRows().length, 1);
    firstItem = queryRows()[0]!;
    assertTrue(isVisible(firstItem));
    assertTrue(isVisible(infiniteList.$.container));
  });
});
