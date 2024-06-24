// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://compare/product_selector.js';

import type {ProductSelectorElement} from 'chrome://compare/product_selector.js';
import {BrowserProxyImpl} from 'chrome://resources/cr_components/commerce/browser_proxy.js';
import {stringToMojoUrl} from 'chrome://resources/js/mojo_type_util.js';
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
    const productTabs = [{
      title: 'title',
      url: stringToMojoUrl('http://example.com'),
    }];
    const recentlyViewedTabs = [{
      title: 'title2',
      url: stringToMojoUrl('http://example2.com'),
    }];
    shoppingServiceApi.setResultFor(
        'getUrlInfosForProductTabs', Promise.resolve({urlInfos: productTabs}));
    shoppingServiceApi.setResultFor(
        'getUrlInfosForRecentlyViewedTabs',
        Promise.resolve({urlInfos: recentlyViewedTabs}));
  }

  setup(async () => {
    shoppingServiceApi.reset();
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    BrowserProxyImpl.setInstance(shoppingServiceApi);
  });

  test('menu shown on enter', async () => {
    initUrlInfos();
    const selector = await createSelector();
    const showingMenuClass = 'showing-menu';
    const menu = selector.$.productSelectionMenu;

    assertEquals(menu.$.menu.getIfExists(), null);
    assertFalse(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));

    selector.$.currentProductContainer.dispatchEvent(
        new KeyboardEvent('keydown', {key: 'Enter'}));
    await flushTasks();

    assertNotEquals(menu.$.menu.getIfExists(), null);
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

    selector.$.productSelectionMenu.close();
    await eventToPromise('close', selector.$.productSelectionMenu);

    assertFalse(selector.$.currentProductContainer.classList.contains(
        showingMenuClass));
  });
});
