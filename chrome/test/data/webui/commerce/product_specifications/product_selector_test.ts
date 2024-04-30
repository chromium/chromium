// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_selector.js';

import type {ProductSelectorElement} from 'chrome://compare/product_selector.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {stringToMojoString16, stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('ProductSelectorTest', () => {
  const shoppingServiceApi = TestMock.fromClass(BrowserProxyImpl);

  async function createSelector(): Promise<ProductSelectorElement> {
    const selector = document.createElement('product-selector');
    selector.selectedItem = {
      title: 'title',
      url: 'https://current-selection.com',
      imageUrl: 'https://current-selection-image.com',
    };
    document.body.appendChild(selector);
    await flushTasks();
    return selector;
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
    const selector = await createSelector();

    shoppingServiceApi.setResultFor(
        'getUrlInfosForOpenTabs', Promise.resolve({urlInfos: openTabs}));

    assertEquals(0, shoppingServiceApi.getCallCount('getUrlInfosForOpenTabs'));

    selector.$.currentProductContainer.click();

    await shoppingServiceApi.whenCalled('getUrlInfosForOpenTabs');

    await flushTasks();

    // Ensure the number of list items is equal to the number of open tabs.
    assertEquals(openTabs.length, selector.openTabs.length);

    assertEquals(titleString, selector.openTabs[0]!.title);
    assertEquals(openTabs[0]!.url.url, selector.openTabs[0]!.url);
  });

  test('menu shown on enter', async () => {
    initUrlInfos();
    const selector = await createSelector();
    const showingMenuClass = 'showing-menu';

    assertEquals(selector.$.productSelectionMenu.getIfExists(), null);
    assertFalse(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));

    selector.$.currentProductContainer.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    assertNotEquals(selector.$.productSelectionMenu.getIfExists(), null);
    assertTrue(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));
  });

  test('updates showing menu class', async () => {
    initUrlInfos();
    const selector = await createSelector();
    const showingMenuClass = 'showing-menu';

    assertFalse(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));

    selector.$.currentProductContainer.click();
    await flushTasks();

    assertTrue(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));

    selector.$.productSelectionMenu.get().close();
    await eventToPromise('close', selector.$.productSelectionMenu.get());

    assertFalse(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));
  });

  test('abbreviates URLs', async () => {
    initUrlInfos();
    const selector = await createSelector();
    selector.$.currentProductContainer.click();
    await flushTasks();

    const menu = selector.$.productSelectionMenu.get();
    const listElement = menu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!listElement);

    const tabUrl = listElement.shadowRoot!.querySelector<HTMLElement>(
        '.description-text');
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

    const selector = await createSelector();
    selector.$.currentProductContainer.click();
    await flushTasks();

    const menu = selector.$.productSelectionMenu.get();
    const listElements = menu.querySelectorAll<HTMLElement>('.dropdown-item');
    assertEquals(1, listElements.length);

    const tabUrl = listElements[0]!.shadowRoot!.querySelector<HTMLElement>(
        '.description-text');
    assertTrue(!!tabUrl);
    assertEquals('example.com', tabUrl.textContent);
  });

  test('fires selector event', async () => {
    initUrlInfos();
    const selector = await createSelector();
    selector.$.currentProductContainer.click();
    await flushTasks();

    const menu = selector.$.productSelectionMenu.get();
    assertTrue(menu.open);
    const listElement = menu.querySelector<HTMLElement>('.dropdown-item');
    assertTrue(!!listElement);
    const eventPromise = eventToPromise('selected-url-change', selector);
    listElement.click();
    const event = await eventPromise;

    assertTrue(!!event);
    assertEquals('http://example.com', event.detail.url);
    assertFalse(menu.open);
  });
});
