// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDeepActiveElement} from 'chrome://resources/js/util.js';
import type {ProfileData, TabSearchPageElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {SelectableLazyListElement, TabSearchApiProxyImpl, TabSearchItemElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertGT, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {keyDownOn} from 'chrome://webui-test/keyboard_mock_interactions.js';
import {eventToPromise, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createProfileData, generateSampleDataFromSiteNames, generateSampleRecentlyClosedTabs, generateSampleTabsFromSiteNames, sampleSiteNames, sampleToken} from './tab_search_test_data.js';
import {assertTabItemAndNeighborsInViewBounds, assertTabItemInViewBounds, disableAnimationBehavior, getStylePropertyPixelValue, initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchAppFocusTest', () => {
  let tabSearchPage: TabSearchPageElement;
  let testProxy: TestTabSearchApiProxy;

  disableAnimationBehavior(SelectableLazyListElement, 'scrollTo');
  disableAnimationBehavior(TabSearchItemElement, 'scrollIntoView');

  async function setupTest(
      sampleData: ProfileData,
      loadTimeOverriddenData?: {[key: string]: string}) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileData(sampleData);
    TabSearchApiProxyImpl.setInstance(testProxy);

    initLoadTimeDataWithDefaults(loadTimeOverriddenData);

    tabSearchPage = document.createElement('tab-search-page');
    document.body.appendChild(tabSearchPage);
    await eventToPromise('viewport-filled', tabSearchPage.$.tabsList);
    await microtasksFinished();
  }

  function queryRows() {
    return tabSearchPage.$.tabsList.querySelectorAll('tab-search-item');
  }

  function queryListTitle(): NodeListOf<HTMLElement> {
    return tabSearchPage.$.tabsList.querySelectorAll('.list-section-title');
  }

  test('KeyNavigation', async () => {
    await setupTest(createProfileData());

    // Initially, the search input should have focus.
    const searchInput = tabSearchPage.$.searchInput;
    assertEquals(searchInput, getDeepActiveElement());

    const tabSearchItems = queryRows();
    tabSearchItems[0]!.focus();
    // Once an item is focused, arrow keys should change focus too.
    keyDownOn(tabSearchItems[0]!, 0, [], 'ArrowDown');
    await eventToPromise('selected-change', tabSearchPage.$.tabsList);
    await microtasksFinished();
    assertEquals(tabSearchItems[1], getDeepActiveElement());

    keyDownOn(tabSearchItems[1]!, 0, [], 'ArrowUp');
    await eventToPromise('selected-change', tabSearchPage.$.tabsList);
    await microtasksFinished();
    assertEquals(tabSearchItems[0], getDeepActiveElement());

    keyDownOn(tabSearchItems[1]!, 0, [], 'End');
    await eventToPromise('selected-change', tabSearchPage.$.tabsList);
    await microtasksFinished();
    assertEquals(
        tabSearchItems[tabSearchItems.length - 1], getDeepActiveElement());

    keyDownOn(tabSearchItems[tabSearchItems.length - 1]!, 0, [], 'Home');
    await eventToPromise('selected-change', tabSearchPage.$.tabsList);
    await microtasksFinished();
    assertEquals(tabSearchItems[0], getDeepActiveElement());

    // On restoring focus to the search field, a list item should be selected if
    // available.
    searchInput.focus();
    assertEquals(0, tabSearchPage.getSelectedTabIndex());
  });

  test('KeyPress', async () => {
    await setupTest(createProfileData());

    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-item')!;
    tabSearchItem.focus();

    keyDownOn(tabSearchItem, 0, [], 'Enter');
    await testProxy.whenCalled('switchToTab');
    keyDownOn(tabSearchItem, 0, [], ' ');
    await testProxy.whenCalled('switchToTab');
    assertEquals(2, testProxy.getCallCount('switchToTab'));

    const closeButton =
        tabSearchItem.shadowRoot!.querySelector('#closeButton')!;
    keyDownOn(closeButton, 0, [], 'Enter');
    await testProxy.whenCalled('closeTab');
    assertEquals(1, testProxy.getCallCount('closeTab'));
  });

  test('ListItemFocusRetainedOnItemChanges', async () => {
    const numTabItems = 5;
    await setupTest(
        generateSampleDataFromSiteNames(sampleSiteNames(numTabItems)));

    assertEquals(numTabItems, queryRows().length);

    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-item')!;
    tabSearchItem.focus();

    const closeButton =
        tabSearchItem.shadowRoot!.querySelector<HTMLElement>('#closeButton')!;
    closeButton.focus();

    for (let i = 0; i < numTabItems - 1; i++) {
      testProxy.getCallbackRouterRemote().tabsRemoved({
        tabIds: [(i + 1)],
        recentlyClosedTabs: [],
      });
      await eventToPromise('focus-restored-for-test', tabSearchPage.$.tabsList);
      assertEquals(numTabItems - 1 - i, queryRows().length);
      assertEquals('tab-search-item', getDeepActiveElement()!.localName);
    }
  });

  test('ViewScrolling', async () => {
    await setupTest(generateSampleDataFromSiteNames(sampleSiteNames(10)));

    const tabsDiv = tabSearchPage.$.tabsList;
    // Assert that the tabs are in a overflowing state.
    assertGT(tabsDiv.scrollHeight, tabsDiv.clientHeight);

    const tabItems =
        tabSearchPage.$.tabsList.querySelectorAll('tab-search-item');
    for (let i = 0; i < tabItems.length; i++) {
      tabItems[i]!.focus();
      await microtasksFinished();

      assertEquals(i, tabSearchPage.getSelectedTabIndex());
      assertTabItemAndNeighborsInViewBounds(tabsDiv, tabItems, i);
    }
  });

  test('Search field input element focused when revealed ', async () => {
    await setupTest(createProfileData());

    // Set the current focus to the search input element.
    const searchInput = tabSearchPage.$.searchInput;
    searchInput.focus();
    assertEquals(searchInput, getDeepActiveElement());

    // Focus an item in the list, search input should not be focused.
    const tabSearchItem =
        tabSearchPage.$.tabsList.querySelector('tab-search-item')!;
    tabSearchItem.focus();
    assertEquals(tabSearchItem, getDeepActiveElement());
    assertNotEquals(searchInput, getDeepActiveElement());

    // When hidden visibilitychange should revert focus to the search input.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(searchInput, getDeepActiveElement());

    // When visible the focused element should still be the search input.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await microtasksFinished();
    assertEquals(searchInput, getDeepActiveElement());
  });

  test('Section item visible on recently closed section expand', async () => {
    // A list height that encompasses two title items, and four open tab items,
    // thus ensuring that on adding a recently closed item to the list, it will
    // be outside the visible boundaries.
    const tabItemHeight =
        getStylePropertyPixelValue(tabSearchPage, '--mwb-item-height');
    const titleItemHeight = getStylePropertyPixelValue(
        tabSearchPage, '--mwb-list-section-title-height');
    const windowHeight = (titleItemHeight * 2) + (4 * tabItemHeight);

    await setupTest(createProfileData({
      windows: [{
        active: true,
        height: windowHeight,
        tabs: generateSampleTabsFromSiteNames(sampleSiteNames(4)),
      }],
      recentlyClosedTabs: generateSampleRecentlyClosedTabs(
          'Sample Tab', 1, sampleToken(0n, 1n)),
      recentlyClosedSectionExpanded: false,
    }));

    const recentlyClosedTitleItem = queryListTitle()[1];
    assertTrue(!!recentlyClosedTitleItem);

    const recentlyClosedTitleExpandButton =
        recentlyClosedTitleItem!.querySelector('cr-expand-button');
    assertTrue(!!recentlyClosedTitleExpandButton);

    // Expand the `Recently Closed` section.
    recentlyClosedTitleExpandButton.click();

    await eventToPromise('viewport-filled', tabSearchPage.$.tabsList);
    // Assert that the tabs are in a overflowing state.
    assertGT(
        tabSearchPage.$.tabsList.scrollHeight,
        tabSearchPage.$.tabsList.clientHeight);

    // Assert the first recently closed item is in view bounds.
    const tabItems =
        tabSearchPage.$.tabsList.querySelectorAll('tab-search-item');
    assertTabItemInViewBounds(tabSearchPage.$.tabsList, tabItems[4]!);
  });
});
