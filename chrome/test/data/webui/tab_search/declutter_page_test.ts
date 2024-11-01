// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {DeclutterPageElement, Tab} from 'chrome://tab-search.top-chrome/tab_search.js';
import {TabSearchApiProxyImpl} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {createTab} from './tab_search_test_data.js';
import {TestTabSearchApiProxy} from './test_tab_search_api_proxy.js';

suite('DeclutterPageTest', () => {
  let declutterPage: DeclutterPageElement;
  let testApiProxy: TestTabSearchApiProxy;

  function declutterPageSetup(tabCount: number = 3) {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    testApiProxy = new TestTabSearchApiProxy();
    const staleTabs = createStaleTabs(tabCount);
    testApiProxy.setStaleTabs(staleTabs);
    TabSearchApiProxyImpl.setInstance(testApiProxy);

    declutterPage = document.createElement('declutter-page');
    document.body.appendChild(declutterPage);
    return microtasksFinished();
  }

  function createStaleTabs(count: number): Tab[] {
    const tabList: Tab[] = [];
    for (let i = 0; i < count; i++) {
      tabList.push(
          createTab({title: 'Tab ' + count, url: {url: 'https://tab.com/'}}));
    }
    return tabList;
  }

  test('Shows correct tab count', async () => {
    const tabCount = 3;
    await declutterPageSetup(tabCount);
    assertEquals(1, testApiProxy.getCallCount('getStaleTabs'));
    const staleTabElements =
        declutterPage.shadowRoot!.querySelectorAll('tab-search-item');
    assertEquals(tabCount, staleTabElements.length);
  });

  test('Closes tabs', async () => {
    await declutterPageSetup();
    assertEquals(0, testApiProxy.getCallCount('declutterTabs'));

    const staleTabElements =
        declutterPage.shadowRoot!.querySelectorAll('tab-search-item');
    const closeButton = declutterPage.shadowRoot!.querySelector('cr-button');
    assertTrue(!!closeButton);
    closeButton.click();

    const [tabIds] = await testApiProxy.whenCalled('declutterTabs');
    assertEquals(staleTabElements.length, tabIds.length);
  });

  test('Excludes from stale tabs', async () => {
    await declutterPageSetup();
    assertEquals(0, testApiProxy.getCallCount('excludeFromStaleTabs'));

    const staleTabElement =
        declutterPage.shadowRoot!.querySelector('tab-search-item');
    assertTrue(!!staleTabElement);
    const removeButton =
        staleTabElement.shadowRoot!.querySelector('cr-icon-button');
    assertTrue(!!removeButton);
    removeButton.click();

    const [tabId] = await testApiProxy.whenCalled('excludeFromStaleTabs');
    assertEquals(staleTabElement.data.tab.tabId, tabId);
  });

  test('Shows tab list on nonzero stale tabs', async () => {
    await declutterPageSetup(1);
    const tabList = declutterPage.shadowRoot!.querySelector('#staleTabList');
    assertTrue(!!tabList);
    assertTrue(isVisible(tabList));
    const emptyState =
        declutterPage.shadowRoot!.querySelector('.empty-content');
    assertFalse(!!emptyState);
  });

  test('Shows empty state on zero stale tabs', async () => {
    await declutterPageSetup(0);
    const tabList = declutterPage.shadowRoot!.querySelector('#staleTabList');
    assertFalse(!!tabList);
    const emptyState =
        declutterPage.shadowRoot!.querySelector('.empty-content');
    assertTrue(!!emptyState);
    assertTrue(isVisible(emptyState));
  });

  test('Focus gives item selected class', async () => {
    await declutterPageSetup(2);

    const tabRows =
        declutterPage.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(2, tabRows.length);

    const closeButton0 =
        tabRows[0]!.shadowRoot!.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton0);
    const closeButton1 =
        tabRows[1]!.shadowRoot!.querySelector(`cr-icon-button`);
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
        declutterPage.shadowRoot!.querySelectorAll('tab-search-item');
    assertTrue(!!tabRows);
    assertEquals(3, tabRows.length);

    const closeButton0 =
        tabRows[0]!.shadowRoot!.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton0);
    const closeButton1 =
        tabRows[1]!.shadowRoot!.querySelector(`cr-icon-button`);
    assertTrue(!!closeButton1);
    const closeButton2 =
        tabRows[2]!.shadowRoot!.querySelector(`cr-icon-button`);
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
