// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';
import 'chrome://read-later.top-chrome/bookmarks/commerce/shopping_list.js';

import {ActionSource} from 'chrome://read-later.top-chrome/bookmarks/bookmarks.mojom-webui.js';
import {BookmarksApiProxyImpl} from 'chrome://read-later.top-chrome/bookmarks/bookmarks_api_proxy.js';
import {ShoppingListElement} from 'chrome://read-later.top-chrome/bookmarks/commerce/shopping_list.js';
import {BookmarkProductInfo} from 'chrome://read-later.top-chrome/bookmarks/commerce/shopping_list.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, isVisible} from 'chrome://webui-test/test_util.js';

import {TestBookmarksApiProxy} from '../test_bookmarks_api_proxy.js';

suite('SidePanelShoppingListTest', () => {
  let shoppingList: ShoppingListElement;
  let bookmarksApi: TestBookmarksApiProxy;

  const products: BookmarkProductInfo[] = [
    {
      bookmarkId: BigInt(3),
      info: {
        title: 'Product Foo',
        domain: 'foo.com',
        imageUrl: {url: 'https://foo.com/image'},
        productUrl: {url: 'https://foo.com/product'},
        currentPrice: '$12',
        previousPrice: '$34',
      },
    },
    {
      bookmarkId: BigInt(4),
      info: {
        title: 'Product bar',
        domain: 'bar.com',
        imageUrl: {url: ''},
        productUrl: {url: 'https://foo.com/product'},
        currentPrice: '$15',
        previousPrice: '',
      },
    },
  ];

  function getProductElements(): HTMLElement[] {
    return Array.from(
        shoppingList.shadowRoot!.querySelectorAll('.product-item'));
  }

  function checkProductElementRender(
      element: HTMLElement, product: BookmarkProductInfo): void {
    assertEquals(
        product.info.title,
        element.querySelector('.product-title')!.textContent);
    assertEquals(
        product.info.domain,
        element.querySelector('.product-domain')!.textContent);

    const imageElement = element.querySelector<HTMLElement>('.product-image');
    const faviconElement = element.querySelector<HTMLElement>('.favicon-image');
    if (!product.info.imageUrl.url) {
      assertFalse(isVisible(imageElement));
      assertTrue(isVisible(faviconElement));
    } else {
      assertFalse(isVisible(faviconElement));
      assertTrue(isVisible(imageElement));
      assertEquals(
          imageElement!.getAttribute('auto-src'), product.info.imageUrl.url);
    }
    const priceElements = Array.from(element.querySelectorAll('.price'));
    if (!product.info.previousPrice) {
      assertEquals(priceElements.length, 1);
      assertEquals(priceElements[0]!.textContent, product.info.currentPrice);
    } else {
      assertEquals(priceElements.length, 2);
      assertEquals(priceElements[0]!.textContent, product.info.currentPrice);
      assertEquals(priceElements[1]!.textContent, product.info.previousPrice);
    }
  }

  setup(async () => {
    document.body.innerHTML = '';

    bookmarksApi = new TestBookmarksApiProxy();
    BookmarksApiProxyImpl.setInstance(bookmarksApi);

    shoppingList = document.createElement('shopping-list');
    shoppingList.productInfos = products;
    document.body.appendChild(shoppingList);

    await flushTasks();
  });

  test('RenderShoppingList', async () => {
    const productElements = getProductElements();
    assertEquals(2, products.length);

    for (let i = 0; i < products.length; i++) {
      checkProductElementRender(productElements[i]!, products[i]!);
    }
  });

  test('OpensAndClosesShoppingList', async () => {
    const productElements = getProductElements();
    const arrowIcon =
        shoppingList.shadowRoot!.querySelector<HTMLElement>('#arrowIcon')!;
    assertTrue(arrowIcon.hasAttribute('open'));
    for (let i = 0; i < productElements.length; i++) {
      assertTrue(isVisible(productElements[i]!));
    }

    shoppingList.shadowRoot!.querySelector<HTMLElement>('.row')!.click();
    await flushTasks();
    assertFalse(arrowIcon.hasAttribute('open'));
    for (let i = 0; i < productElements.length; i++) {
      assertFalse(isVisible(productElements[i]!));
    }
  });

  test('OpensProductItem', async () => {
    getProductElements()[0]!.click();
    const [id, parentFolderDepth, , source] =
        await bookmarksApi.whenCalled('openBookmark');
    assertEquals(products[0]!.bookmarkId.toString(), id);
    assertEquals(0, parentFolderDepth);
    assertEquals(ActionSource.kPriceTracking, source);
  });

  test('OpensProductItemContextMenu', async () => {
    getProductElements()[0]!.dispatchEvent(new MouseEvent('contextmenu'));
    const [id, , , source] = await bookmarksApi.whenCalled('showContextMenu');
    assertEquals(products[0]!.bookmarkId.toString(), id);
    assertEquals(ActionSource.kPriceTracking, source);
  });

  test('OpensProductItemWithAuxClick', async () => {
    // Middle mouse button click.
    getProductElements()[0]!.dispatchEvent(
        new MouseEvent('auxclick', {button: 1}));
    const [id, parentFolderDepth, , source] =
        await bookmarksApi.whenCalled('openBookmark');
    assertEquals(products[0]!.bookmarkId.toString(), id);
    assertEquals(0, parentFolderDepth);
    assertEquals(ActionSource.kPriceTracking, source);

    bookmarksApi.resetResolver('openBookmark');

    // Non-middle mouse aux clicks.
    getProductElements()[0]!.dispatchEvent(
        new MouseEvent('auxclick', {button: 2}));
    assertEquals(0, bookmarksApi.getCallCount('openBookmark'));
  });
});
