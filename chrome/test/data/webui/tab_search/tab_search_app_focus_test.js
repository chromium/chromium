// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';
import {InfiniteList, ProfileData, TabSearchApiProxyImpl, TabSearchAppElement, TabSearchItem, TabSearchSearchField} from 'chrome://tab-search.top-chrome/tab_search.js';

import {assertEquals, assertGT, assertNotEquals} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';

import {generateSampleDataFromSiteNames, sampleData, sampleSiteNames} from './tab_search_test_data.js';
import {assertTabItemAndNeighborsInViewBounds, disableAnimationBehavior, initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('TabSearchAppFocusTest', () => {
  /** @type {!TabSearchAppElement} */
  let tabSearchApp;
  /** @type {!TestTabSearchApiProxy} */
  let testProxy;

  disableAnimationBehavior(InfiniteList, 'scrollTo');
  disableAnimationBehavior(TabSearchItem, 'scrollIntoView');

  /**
   * @param {ProfileData} sampleData
   * @param {Object=} loadTimeOverriddenData
   */
  async function setupTest(sampleData, loadTimeOverriddenData) {
    testProxy = new TestTabSearchApiProxy();
    testProxy.setProfileData(sampleData);
    TabSearchApiProxyImpl.instance_ = testProxy;

    initLoadTimeDataWithDefaults(loadTimeOverriddenData);

    tabSearchApp = /** @type {!TabSearchAppElement} */
        (document.createElement('tab-search-app'));

    document.body.innerHTML = '';
    document.body.appendChild(tabSearchApp);
    await flushTasks();
  }

  /**
   * @return {!NodeList<!HTMLElement>}
   */
  function queryRows() {
    return /** @type {!NodeList<!HTMLElement>} */ (
        tabSearchApp.shadowRoot.querySelector('#tabsList')
            .querySelectorAll('tab-search-item'));
  }

  test('KeyNavigation', async () => {
    await setupTest(sampleData());

    // Initially, the search input should have focus.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    const searchInput = /** @type {!HTMLInputElement} */
        (searchField.shadowRoot.querySelector('#searchInput'));
    assertEquals(searchInput, getDeepActiveElement());

    const tabSearchItems = queryRows();
    tabSearchItems[0].focus();
    // Once an item is focused, arrow keys should change focus too.
    keyDownOn(tabSearchItems[0], 0, [], 'ArrowDown');
    assertEquals(tabSearchItems[1], getDeepActiveElement());

    keyDownOn(tabSearchItems[1], 0, [], 'ArrowUp');
    assertEquals(tabSearchItems[0], getDeepActiveElement());

    keyDownOn(tabSearchItems[1], 0, [], 'End');
    assertEquals(
        tabSearchItems[tabSearchItems.length - 1], getDeepActiveElement());

    keyDownOn(tabSearchItems[tabSearchItems.length - 1], 0, [], 'Home');
    assertEquals(tabSearchItems[0], getDeepActiveElement());

    // On restoring focus to the search field, a list item should be selected if
    // available.
    searchInput.focus();
    assertEquals(0, tabSearchApp.getSelectedIndex());
  });

  test('KeyPress', async () => {
    await setupTest(sampleData());

    const tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item'));
    tabSearchItem.focus();

    keyDownOn(tabSearchItem, 0, [], 'Enter');
    keyDownOn(tabSearchItem, 0, [], ' ');
    assertEquals(2, testProxy.getCallCount('switchToTab'));

    const closeButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('#closeButton'));
    keyDownOn(closeButton, 0, [], 'Enter');
    assertEquals(1, testProxy.getCallCount('closeTab'));
  });

  test('ListItemFocusRetainedOnItemChanges', async () => {
    const numTabItems = 5;
    await setupTest(
        generateSampleDataFromSiteNames(sampleSiteNames(numTabItems)));

    await waitAfterNextRender(tabSearchApp);
    assertEquals(numTabItems, queryRows().length);

    const tabSearchItem = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelector('tab-search-item'));
    tabSearchItem.focus();

    const closeButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('#closeButton'));
    closeButton.focus();

    for (let i = 0; i < numTabItems - 1; i++) {
      testProxy.getCallbackRouterRemote().tabsRemoved([i + 1]);
      await waitAfterNextRender(tabSearchApp);
      assertEquals(numTabItems - 1 - i, queryRows().length);
      assertEquals('tab-search-item', getDeepActiveElement().localName);
    }
  });

  test('ViewScrolling', async () => {
    await setupTest(generateSampleDataFromSiteNames(sampleSiteNames(10)));

    const tabsDiv = /** @type {!HTMLElement} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList'));
    // Assert that the tabs are in a overflowing state.
    assertGT(tabsDiv.scrollHeight, tabsDiv.clientHeight);

    const tabItems = /** @type {!NodeList<HTMLElement>} */
        (tabSearchApp.shadowRoot.querySelector('#tabsList')
             .querySelectorAll('tab-search-item'));
    for (let i = 0; i < tabItems.length; i++) {
      tabItems[i].focus();

      assertEquals(i, tabSearchApp.getSelectedIndex());
      assertTabItemAndNeighborsInViewBounds(tabsDiv, tabItems, i);
    }
  });

  test('Search field input element focused when revealed ', async () => {
    await setupTest(sampleData());

    // Set the current focus to the search input element.
    const searchField = /** @type {!TabSearchSearchField} */
        (tabSearchApp.shadowRoot.querySelector('#searchField'));
    const searchInput = /** @type {!HTMLInputElement} */
        (searchField.shadowRoot.querySelector('#searchInput'));
    searchInput.focus();
    assertEquals(searchInput, getDeepActiveElement());

    // Focus an item in the list, search input should not be focused.
    const tabSearchItem = /** @type {!HTMLElement} */ (
        tabSearchApp.shadowRoot.querySelector('#tabsList')
            .querySelector('tab-search-item'));
    tabSearchItem.focus();
    assertEquals(tabSearchItem, getDeepActiveElement());
    assertNotEquals(searchInput, getDeepActiveElement());

    // When hidden visibilitychange should revert focus to the search input.
    Object.defineProperty(
        document, 'visibilityState', {value: 'hidden', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(searchInput, getDeepActiveElement());

    // When visible the focused element should still be the search input.
    Object.defineProperty(
        document, 'visibilityState', {value: 'visible', writable: true});
    document.dispatchEvent(new Event('visibilitychange'));
    await flushTasks();
    assertEquals(searchInput, getDeepActiveElement());
  });

});
