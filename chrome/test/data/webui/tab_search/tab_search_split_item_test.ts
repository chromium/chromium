// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import type {TabSearchSplitItemElement} from 'chrome://tab-search.top-chrome/tab_search.js';
import {SplitTabLayout, SplitViewData, TabGroupColor} from 'chrome://tab-search.top-chrome/tab_search.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {sampleToken} from './tab_search_test_data.js';
import {initLoadTimeDataWithDefaults} from './tab_search_test_helper.js';

suite('TabSearchSplitItemTest', () => {
  let tabSearchSplitItem: TabSearchSplitItemElement;

  function setupTest(
      data: SplitViewData,
      loadTimeOverriddenData?: {[key: string]: number|string|boolean}) {
    initLoadTimeDataWithDefaults(loadTimeOverriddenData);
    tabSearchSplitItem = document.createElement('tab-search-split-item');
    tabSearchSplitItem.data = data;
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    document.body.appendChild(tabSearchSplitItem);
  }

  test('LayoutRendering', async () => {
    const token = sampleToken(1n, 1n);
    const dataSideBySide = new SplitViewData({
      splitView: {
        sessionId: -1,
        id: token,
        tabCount: 2,
        lastActiveTime: {internalValue: 0n},
        lastActiveElapsedText: '2 mins ago',
        tabUrls: ['https://google.com', 'https://paypal.com'],
        layout: SplitTabLayout.kSideBySide,
        groupId: null,
      },
    });

    setupTest(dataSideBySide);
    await tabSearchSplitItem.updateComplete;

    const faviconsElement =
        tabSearchSplitItem.shadowRoot.querySelector('.split-favicons')!;
    assertNotEquals(null, faviconsElement);
    assertTrue(faviconsElement.classList.contains('side-by-side'));
    assertFalse(faviconsElement.classList.contains('stacked'));

    const dataStacked = new SplitViewData({
      splitView: {
        sessionId: -1,
        id: token,
        tabCount: 2,
        lastActiveTime: {internalValue: 0n},
        lastActiveElapsedText: '2 mins ago',
        tabUrls: ['https://google.com', 'https://paypal.com'],
        layout: SplitTabLayout.kStacked,
        groupId: null,
      },
    });

    setupTest(dataStacked);
    await tabSearchSplitItem.updateComplete;

    const faviconsElementStacked =
        tabSearchSplitItem.shadowRoot.querySelector('.split-favicons')!;
    assertFalse(faviconsElementStacked.classList.contains('side-by-side'));
    assertTrue(faviconsElementStacked.classList.contains('stacked'));
  });

  test('DomainExtractionAndElision', async () => {
    const token = sampleToken(1n, 1n);
    const data = new SplitViewData({
      splitView: {
        sessionId: -1,
        id: token,
        tabCount: 2,
        lastActiveTime: {internalValue: 0n},
        lastActiveElapsedText: '2 mins ago',
        tabUrls: ['view-source:https://example.com', 'file:///etc/passwd'],
        layout: SplitTabLayout.kSideBySide,
        groupId: null,
      },
    });

    setupTest(data);
    await tabSearchSplitItem.updateComplete;

    const domains =
        tabSearchSplitItem.shadowRoot.querySelectorAll('.domain-text bdi');
    assertEquals(2, domains.length);
    assertEquals('view source', domains[0]!.textContent);
    assertEquals('local or shared file', domains[1]!.textContent);
  });

  test('GroupDetailsPresence', async () => {
    const token = sampleToken(1n, 1n);
    const data = new SplitViewData({
      splitView: {
        sessionId: -1,
        id: token,
        tabCount: 2,
        lastActiveTime: {internalValue: 0n},
        lastActiveElapsedText: '2 mins ago',
        tabUrls: ['https://google.com', 'https://paypal.com'],
        layout: SplitTabLayout.kSideBySide,
        groupId: null,
      },
    });

    data.tabGroup = {
      id: token,
      color: TabGroupColor.kBlue,
      title: 'Work Group',
    };

    setupTest(data);
    await tabSearchSplitItem.updateComplete;

    const groupSvgElement =
        tabSearchSplitItem.shadowRoot.querySelector<HTMLElement>('#groupSvg')!;
    assertNotEquals(null, groupSvgElement);

    const useColorRefresh = loadTimeData.getBoolean('useTabGroupColorRefresh');
    const expectedColorVar = useColorRefresh ?
        'var(--tab-group-refresh-color-blue)' :
        'var(--tab-group-color-blue)';

    assertEquals(
        expectedColorVar,
        groupSvgElement.style.getPropertyValue('--group-dot-color'));
  });
});
