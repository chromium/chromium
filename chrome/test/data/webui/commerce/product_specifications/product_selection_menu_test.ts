// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_selection_menu.js';

import type {ProductSelectionMenuElement} from 'chrome://compare/product_selection_menu.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

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
    const titleString = 'title';
    const openTabs = [{
      title: stringToMojoString16(titleString),
      url: stringToMojoUrl('http://example.com'),
    }];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForOpenTabs', Promise.resolve({urlInfos: openTabs}));
  }

  setup(async () => {
    shoppingServiceApi.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
  });

  test('open tabs shown', async () => {
    const titleString = 'title';
    const openTabs = [{
      title: titleString,
      url: stringToMojoUrl('http://example.com'),
    }];
    const menu = await createMenu();
    shoppingServiceApi.setResultFor(
        'getUrlInfosForOpenTabs', Promise.resolve({urlInfos: openTabs}));
    assertEquals(0, shoppingServiceApi.getCallCount('getUrlInfosForOpenTabs'));

    menu.showAt(document.body);
    await shoppingServiceApi.whenCalled('getUrlInfosForOpenTabs');

    // Ensure the number of list items is equal to the number of open tabs.
    assertEquals(openTabs.length, menu.openTabs.length);
    assertEquals(titleString, menu.openTabs[0]!.title);
    assertEquals(openTabs[0]!.url.url, menu.openTabs[0]!.url);
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
    const titleString = 'title';
    const openTabs = [
      {
        title: stringToMojoString16(titleString),
        url: stringToMojoUrl('https://example.com'),
      },
      {
        title: stringToMojoString16(titleString),
        url: stringToMojoUrl('https://current-selection.com'),
      },
    ];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForOpenTabs', Promise.resolve({urlInfos: openTabs}));

    const menu = await createMenu();
    menu.selectedUrl = 'https://current-selection.com';
    menu.showAt(document.body);
    await flushTasks();

    const listElements =
        menu.$.menu.get().querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(1, listElements.length);

    const tabUrl = listElements[0]!.shadowRoot!.querySelector<HTMLElement>(
        '.description-text');
    assertTrue(!!tabUrl);
    assertEquals('example.com', tabUrl.textContent);
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
});
