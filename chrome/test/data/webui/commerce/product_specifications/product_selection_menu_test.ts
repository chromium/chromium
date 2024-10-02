// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_selection_menu.js';

import type {ProductSelectionMenuElement} from 'chrome://compare/product_selection_menu.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise, isVisible} from 'chrome://webui-test/test_util.js';

import {$$, assertNotStyle, assertStyle} from './test_support.js';

suite('ProductSelectionMenuTest', () => {
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  async function createMenu(): Promise<ProductSelectionMenuElement> {
    const menu = document.createElement('product-selection-menu');
    menu.selectedUrl = 'https://current-selection.com';
    document.body.appendChild(menu);
    await flushTasks();
    return menu;
  }

  function initUrlInfos() {
    initProductTabUrlInfos();
    initRecentlyViewedTabUrlInfos();
  }

  function initProductTabUrlInfos(productTabs = [{
                                    title: 'title',
                                    url: stringToMojoUrl('http://example.com'),
                                  }]) {
    shoppingServiceApi.setResultFor(
        'getUrlInfosForProductTabs', Promise.resolve({urlInfos: productTabs}));
  }

  function initRecentlyViewedTabUrlInfos(
      recentlyViewedTabs = [{
        title: 'title2',
        url: stringToMojoUrl('http://example2.com'),
      }]) {
    shoppingServiceApi.setResultFor(
        'getUrlInfosForRecentlyViewedTabs',
        Promise.resolve({urlInfos: recentlyViewedTabs}));
  }

  setup(async () => {
    shoppingServiceApi.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
    loadTimeData.overrideValues({
      suggestedTabs: 'suggestions',
      recentlyViewedTabs: 'recently viewed tabs',
    });
  });

  test('empty state shown', async () => {
    initRecentlyViewedTabUrlInfos([]);
    initProductTabUrlInfos([]);

    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    assertEquals(0, menu.sections.length);
    assertNotStyle($$(menu, '#empty')!, 'display', 'none');
    assertFalse(!!$$(menu, '.section-title'));
  });

  test('Product tabs shown', async () => {
    initRecentlyViewedTabUrlInfos([]);
    const title = 'title';
    const url = stringToMojoUrl('http://example.com');
    const productTabs = [{
      title: title,
      url: url,
    }];
    initProductTabUrlInfos(productTabs);

    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    assertStyle($$(menu, '#empty')!, 'display', 'none');
    const sectionTitles = menu.shadowRoot!.querySelectorAll('.section-title');
    assertEquals(1, sectionTitles.length);
    assertEquals('suggestions', sectionTitles[0]!.textContent);
    // Ensure the number of open tab list items is equal to the number of open
    // tabs.
    assertEquals(1, menu.sections.length);
    const menuOpenTabEntries = menu.sections[0]!.entries;
    assertEquals(productTabs.length, menuOpenTabEntries.length);
    assertEquals(title, menuOpenTabEntries[0]!.title);
    assertEquals(url.url, menuOpenTabEntries[0]!.url);
  });

  test('recently viewed tabs shown', async () => {
    initProductTabUrlInfos([]);
    const title = 'title2';
    const url = stringToMojoUrl('http://example2.com');
    const recentlyViewedTabs = [{
      title: title,
      url: url,
    }];
    initRecentlyViewedTabUrlInfos(recentlyViewedTabs);

    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    assertStyle($$(menu, '#empty')!, 'display', 'none');
    const sectionTitles = menu.shadowRoot!.querySelectorAll('.section-title');
    assertEquals(1, sectionTitles.length);
    assertEquals('recently viewed tabs', sectionTitles[0]!.textContent);
    // Ensure the number of recently viewed list items is equal to the number
    // of recently viewed tabs.
    assertEquals(1, menu.sections.length);
    const recentlyViewedTabEntries = menu.sections[0]!.entries;
    assertEquals(recentlyViewedTabs.length, recentlyViewedTabEntries.length);
    assertEquals(title, recentlyViewedTabEntries[0]!.title);
    assertEquals(url.url, recentlyViewedTabEntries[0]!.url);
  });

  test('both open and recently viewed tabs shown', async () => {
    const title1 = 'title1';
    const url = stringToMojoUrl('http://example.com');
    const productTabs = [{
      title: title1,
      url: url,
    }];
    initProductTabUrlInfos(productTabs);
    const title2 = 'title2';
    const recentlyViewedTabs = [{
      title: title2,
      url: url,
    }];
    initRecentlyViewedTabUrlInfos(recentlyViewedTabs);

    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    assertStyle($$(menu, '#empty')!, 'display', 'none');
    const sectionTitles = menu.shadowRoot!.querySelectorAll('.section-title');
    assertEquals(2, sectionTitles.length);
    assertEquals('suggestions', sectionTitles[0]!.textContent);
    assertEquals('recently viewed tabs', sectionTitles[1]!.textContent);
    assertEquals(2, menu.sections.length);
    const menuOpenTabEntries = menu.sections[0]!.entries;
    assertEquals(productTabs.length, menuOpenTabEntries.length);
    assertEquals(title1, menuOpenTabEntries[0]!.title);
    assertEquals(url.url, menuOpenTabEntries[0]!.url);
    const recentlyViewedTabEntries = menu.sections[1]!.entries;
    assertEquals(recentlyViewedTabs.length, recentlyViewedTabEntries.length);
    assertEquals(title2, recentlyViewedTabEntries[0]!.title);
    assertEquals(url.url, recentlyViewedTabEntries[0]!.url);
  });

  test('abbreviates URLs', async () => {
    initUrlInfos();
    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    const listElement =
        menu.$.menu.get().querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!listElement);

    const tabUrl =
        listElement.shadowRoot!.querySelector<HTMLElement>('.description-text');
    assertTrue(!!tabUrl);
    assertEquals('example.com', tabUrl.textContent);
  });

  test('excludes current selection', async () => {
    initRecentlyViewedTabUrlInfos([]);
    const titleString = 'title';
    const productTabs = [
      {
        title: titleString,
        url: stringToMojoUrl('https://example.com'),
      },
      {
        title: titleString,
        url: stringToMojoUrl('https://current-selection.com'),
      },
    ];
    initProductTabUrlInfos(productTabs);

    const menu = await createMenu();
    menu.selectedUrl = 'https://current-selection.com';
    menu.showAt(document.body);
    await flushTasks();

    const listElements =
        menu.$.menu.get().querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(2, listElements.length);

    const tabUrl = listElements[0]!.shadowRoot!.querySelector<HTMLElement>(
        '.description-text');
    assertTrue(!!tabUrl);
    assertEquals('example.com', tabUrl.textContent);
  });

  test('excludes excluded urls', async () => {
    const titleString = 'title';
    const excludedUrlString1 = 'https://excluded-url-1.com';
    const excludedUrlString2 = 'https://excluded-url-2.com';
    const recentlyViewedTabs = [
      {
        title: titleString,
        url: stringToMojoUrl(excludedUrlString1),
      },
    ];
    initRecentlyViewedTabUrlInfos(recentlyViewedTabs);
    const productTabs = [
      {
        title: titleString,
        url: stringToMojoUrl('https://example.com'),
      },
      {
        title: titleString,
        url: stringToMojoUrl(excludedUrlString2),
      },
    ];
    initProductTabUrlInfos(productTabs);

    const menu = await createMenu();
    menu.excludedUrls = [excludedUrlString1, excludedUrlString2];
    menu.showAt(document.body);
    await flushTasks();

    const listElements =
        menu.$.menu.get().querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(2, listElements.length);

    const tabUrl = listElements[0]!.shadowRoot!.querySelector<HTMLElement>(
        '.description-text');
    assertTrue(!!tabUrl);
    assertEquals('example.com', tabUrl.textContent);
  });

  test('shows table too large message on new column only', async () => {
    initUrlInfos();
    const menu = await createMenu();
    menu.selectedUrl = '';
    menu.excludedUrls = [];
    menu.forNewColumn = true;
    menu.isTableFull = true;
    menu.showAt(document.body);
    await flushTasks();
    await waitAfterNextRender(menu);

    const crActionMenu = menu.$.menu.get();
    assertTrue(crActionMenu.open);

    const tableFullMessage =
        crActionMenu.querySelector<HTMLElement>('#tableFullMessage');
    assertTrue(isVisible(tableFullMessage));

    const emptyMessage = crActionMenu.querySelector<HTMLElement>('#empty');
    assertFalse(isVisible(emptyMessage));

    const removeColumnButton =
        crActionMenu.querySelector<HTMLElement>('#remove');
    assertFalse(isVisible(removeColumnButton));

    // In this state, no URL options should be shown, even if they're available.
    const listElements =
        crActionMenu.querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(0, listElements.length);
  });

  test(
      'shows table too large message not shown for normal column', async () => {
        initUrlInfos();
        const menu = await createMenu();
        menu.selectedUrl = 'https://example.com';
        menu.excludedUrls = ['https://example.com'];
        menu.forNewColumn = false;
        menu.isTableFull = true;
        menu.showAt(document.body);
        await flushTasks();
        await waitAfterNextRender(menu);

        const crActionMenu = menu.$.menu.get();
        assertTrue(crActionMenu.open);

        const tableFullMessage =
            crActionMenu.querySelector<HTMLElement>('#tableFullMessage');
        assertFalse(isVisible(tableFullMessage));

        const emptyMessage = crActionMenu.querySelector<HTMLElement>('#empty');
        assertFalse(isVisible(emptyMessage));

        const removeColumnButton =
            crActionMenu.querySelector<HTMLElement>('#remove');
        assertTrue(isVisible(removeColumnButton));

        // In this state, URL options should be shown even if the table is full.
        const listElements =
            crActionMenu.querySelectorAll<HTMLElement>('.dropdown-item');
        assertNotEquals(0, listElements.length);
      });

  test('fires selector event', async () => {
    initUrlInfos();
    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    const crActionMenu = menu.$.menu.get();
    assertTrue(crActionMenu.open);
    const listElement =
        crActionMenu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!listElement);
    const eventPromise = eventToPromise('selected-url-change', menu);
    listElement.click();
    const event = await eventPromise;

    assertTrue(!!event);
    assertEquals('http://example.com', event.detail.url);
    assertFalse(crActionMenu.open);
  });

  test('fires removal event', async () => {
    initUrlInfos();
    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    const crActionMenu = menu.$.menu.get();
    assertTrue(crActionMenu.open);
    const removeButton = crActionMenu.querySelector<HTMLElement>('#remove');
    assertTrue(!!removeButton);
    const eventPromise = eventToPromise('remove-url', menu);
    removeButton.click();
    const event = await eventPromise;

    assertTrue(!!event);
    assertFalse(crActionMenu.open);
  });

  test('updates when infos change', async () => {
    initRecentlyViewedTabUrlInfos([]);
    initProductTabUrlInfos([]);

    const menu = await createMenu();
    menu.showAt(document.body);
    await flushTasks();

    assertFalse(!!$$(menu, '.section-title'));

    const title = 'title';
    const url = stringToMojoUrl('http://example.com');
    const productTabs = [{
      title: title,
      url: url,
    }];
    initProductTabUrlInfos(productTabs);

    menu.showAt(document.body);
    await flushTasks();

    const sectionTitles = menu.shadowRoot!.querySelectorAll('.section-title');
    assertEquals(1, sectionTitles.length);
    assertEquals('suggestions', sectionTitles[0]!.textContent);
    const menuOpenTabEntries = menu.sections[0]!.entries;
    assertEquals(productTabs.length, menuOpenTabEntries.length);
    assertEquals(title, menuOpenTabEntries[0]!.title);
    assertEquals(url.url, menuOpenTabEntries[0]!.url);
  });
});
