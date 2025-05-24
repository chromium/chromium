// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {DeclutterPageElement, Tab, TabSearchItemElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {getAnnouncerInstance, TabSearchApiProxyImpl, TIMEOUT_MS} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTab} from './tab_search_test_data.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('DeclutterPageTest', () => {
  let declutterPage: DeclutterPageElement;
  let testApiProxy: TestTabSearchApiProxy;

  function declutterPageSetup(
      staleTabCount: number = 3, duplicateTabCount: number = 0) {
    loadTimeData.overrideValues({
      dedupeEnabled: true,
    });

    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testApiProxy = new TestTabSearchApiProxy();
    const staleTabs = createStaleTabs(staleTabCount);
    testApiProxy.setStaleTabs(staleTabs);
    const duplicateTabs = createDuplicateTabs(duplicateTabCount, 3);
    testApiProxy.setDuplicateTabs(duplicateTabs);
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    declutterPage = document.createElement('declutter-page');
    document.body.appendChild(declutterPage);
    return microtasksFinished();
  }

  function createStaleTabs(count: number): Tab[] {
    const tabList: Tab[] = [];
    for (let i = 0; i < count; i++) {
      tabList.push(
          createTab({title: 'Tab ' + i, url: {url: 'https://tab.com/'}}));
    }
    return tabList;
  }

  function createDuplicateTabs(
      listCount: number, duplicateCount: number): {[key: string]: Tab[]} {
    const tabMap: {[key: string]: Tab[]} = {};
    for (let i = 0; i < listCount; i++) {
      const url = 'https://tab.com/' + i;
      const tabList: Tab[] = [];
      for (let j = 0; j < duplicateCount; j++) {
        tabList.push(createTab({title: 'Tab ' + j, url: {url: url}}));
      }
      tabMap[url] = tabList;
    }
    return tabMap;
  }

  test('Shows correct tab count', async () => {
    const staleTabCount = 3;
    const duplicateTabCount = 4;
    await declutterPageSetup(staleTabCount, duplicateTabCount);
    assertEquals(1, testApiProxy.getCallCount('getUnusedTabs'));
    const staleTabElements = declutterPage.shadowRoot.querySelectorAll(
        '#staleTabList > tab-search-item');
    assertEquals(staleTabCount, staleTabElements.length);
    const duplicateTabElements = declutterPage.shadowRoot.querySelectorAll(
        '#duplicateTabList > tab-search-item');
    assertEquals(duplicateTabCount, duplicateTabElements.length);
  });

  test('Closes tabs', async () => {
    await declutterPageSetup(3, 4);
    assertEquals(0, testApiProxy.getCallCount('declutterTabs'));

    const staleTabElements = declutterPage.shadowRoot.querySelectorAll(
        '#staleTabList > tab-search-item');
    const duplicateTabElements = declutterPage.shadowRoot.querySelectorAll(
        '#duplicateTabList > tab-search-item');
    const closeButton = declutterPage.shadowRoot.querySelector('cr-button');
    assertTrue(!!closeButton);
    closeButton.click();

    const [tabIds, urls] = await testApiProxy.whenCalled('declutterTabs');
    assertEquals(staleTabElements.length, tabIds.length);
    assertEquals(duplicateTabElements.length, urls.length);
  });

  test('Excludes from stale tabs', async () => {
    const announcement = 'Announcement';
    loadTimeData.overrideValues({
      a11yTabExcludedFromList: announcement,
    });

    await declutterPageSetup();
    assertEquals(0, testApiProxy.getCallCount('excludeFromStaleTabs'));

    const staleTabElement =
        declutterPage.shadowRoot.querySelector<TabSearchItemElement>(
            '#staleTabList > tab-search-item');
    assertTrue(!!staleTabElement);
    const removeButton =
        staleTabElement.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!removeButton);
    removeButton.click();

    const [tabId] = await testApiProxy.whenCalled('excludeFromStaleTabs');
    assertEquals(staleTabElement.data.tab.tabId, tabId);

    await new Promise(resolve => setTimeout(resolve, TIMEOUT_MS));
    const announcer = getAnnouncerInstance();
    assertEquals(
        announcement,
        announcer.shadowRoot!.querySelector('#messages')!.textContent);
  });

  test('Excludes from duplicate tabs', async () => {
    await declutterPageSetup(3, 3);
    assertEquals(0, testApiProxy.getCallCount('excludeFromDuplicateTabs'));

    const duplicateTabElement =
        declutterPage.shadowRoot.querySelector<TabSearchItemElement>(
            '#duplicateTabList > tab-search-item');
    assertTrue(!!duplicateTabElement);
    const removeButton =
        duplicateTabElement.shadowRoot.querySelector('cr-icon-button');
    assertTrue(!!removeButton);
    removeButton.click();

    const [url] = await testApiProxy.whenCalled('excludeFromDuplicateTabs');
    assertEquals(duplicateTabElement.data.tab.url, url);
  });

  test('Shows tab list on nonzero tabs', async () => {
    await declutterPageSetup(2);
    const tabList = declutterPage.shadowRoot.querySelector('#staleTabList');
    assertTrue(!!tabList);
    assertTrue(isVisible(tabList));
    const emptyState = declutterPage.shadowRoot.querySelector('.empty-content');
    assertFalse(!!emptyState);
  });

  test('Shows empty state on zero tabs', async () => {
    await declutterPageSetup(0);
    const tabList = declutterPage.shadowRoot.querySelector('#staleTabList');
    assertFalse(!!tabList);
    const emptyState = declutterPage.shadowRoot.querySelector('.empty-content');
    assertTrue(!!emptyState);
    assertTrue(isVisible(emptyState));
  });

  test('Focus gives item selected class', async () => {
    await declutterPageSetup(2);

    const tabRows =
        declutterPage.shadowRoot.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(2, tabRows.length);

    const closeButton0 = tabRows[0]!.shadowRoot.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton0);
    const closeButton1 = tabRows[1]!.shadowRoot.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton1);

    closeButton0.focus();

    assertTrue(tabRows[0]!.classList.contains('selected'));
    assertFalse(tabRows[1]!.classList.contains('selected'));

    closeButton1.focus();

    assertFalse(tabRows[0]!.classList.contains('selected'));
    assertTrue(tabRows[1]!.classList.contains('selected'));
  });

  test('Arrow keys traverse focus in tab list', async () => {
    await declutterPageSetup(3);

    const tabRows =
        declutterPage.shadowRoot.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(3, tabRows.length);

    const closeButton0 = tabRows[0]!.shadowRoot.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton0);
    const closeButton1 = tabRows[1]!.shadowRoot.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton1);
    const closeButton2 = tabRows[2]!.shadowRoot.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton2);

    closeButton0.focus();

    assertTrue(closeButton0.matches(':focus'));
    assertFalse(closeButton1.matches(':focus'));
    assertFalse(closeButton2.matches(':focus'));

    tabRows[0]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowUp'}));
    await microtasksFinished();

    assertFalse(closeButton0.matches(':focus'));
    assertFalse(closeButton1.matches(':focus'));
    assertTrue(closeButton2.matches(':focus'));

    tabRows[2]!.dispatchEvent(new KeyboardEvent('keydown', {key: 'ArrowDown'}));
    await microtasksFinished();

    assertTrue(closeButton0.matches(':focus'));
    assertFalse(closeButton1.matches(':focus'));
    assertFalse(closeButton2.matches(':focus'));
  });
});
